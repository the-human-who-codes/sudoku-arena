import type { LogicStep, SolveResponse } from '../engine/types';
import { clueCount, parsePuzzle, type Values } from '../sudoku/grid';

export interface BoardState {
  values: Uint8Array;
  /** Pencil marks as digit masks; only logic traces track them. */
  candidates: Uint16Array;
}

export type SearchKind = 'guess' | 'forced' | 'undo';

export type StepView =
  | { kind: SearchKind; cell: number; digit: number }
  | { kind: 'logic'; step: LogicStep };

/** A solve, replayable forwards and backwards at a cost proportional to each step's changes. */
export interface Trace {
  readonly length: number;
  readonly showsCandidates: boolean;
  initial(): BoardState;
  /** Applies step `i` to `state` and returns the cells it touched. */
  forward(state: BoardState, i: number): readonly number[];
  /** Reverts step `i`, which must be the last one applied. */
  backward(state: BoardState, i: number): readonly number[];
  view(i: number): StepView;
  /** Filled cells after each step (index 0 is before any step). */
  filledCounts(): Uint8Array;
}

const KINDS: readonly SearchKind[] = ['guess', 'forced', 'undo'];

export const decodeEvent = (event: number) => ({
  kind: KINDS[event >>> 11],
  cell: (event >>> 4) & 0x7f,
  digit: event & 0xf,
});

function emptyState(puzzle: Values): BoardState {
  return { values: Uint8Array.from(puzzle), candidates: new Uint16Array(81) };
}

export function searchTrace(puzzle: Values, events: Uint32Array): Trace {
  return {
    length: events.length,
    showsCandidates: false,
    initial: () => emptyState(puzzle),
    forward(state, i) {
      const { kind, cell, digit } = decodeEvent(events[i]);
      state.values[cell] = kind === 'undo' ? 0 : digit;
      return [cell];
    },
    backward(state, i) {
      const { kind, cell, digit } = decodeEvent(events[i]);
      state.values[cell] = kind === 'undo' ? digit : 0;
      return [cell];
    },
    view: (i) => decodeEvent(events[i]),
    filledCounts() {
      const counts = new Uint8Array(events.length + 1);
      let filled = clueCount(puzzle);
      counts[0] = filled;
      for (let i = 0; i < events.length; i++) {
        filled += events[i] >>> 11 === 2 ? -1 : 1;
        counts[i + 1] = filled;
      }
      return counts;
    },
  };
}

export function logicTrace(puzzle: Values, candidates: number[], steps: LogicStep[]): Trace {
  const start = (): BoardState => ({
    values: Uint8Array.from(puzzle),
    candidates: Uint16Array.from(candidates.length === 81 ? candidates : new Array(81).fill(0)),
  });

  // Record what each step overwrote, so stepping back is as cheap as stepping forward.
  const previous: (typeof steps)[number]['changes'][] = [];
  const counts = new Uint8Array(steps.length + 1);
  const state = start();
  let filled = clueCount(puzzle);
  counts[0] = filled;
  steps.forEach((step, i) => {
    previous.push(step.changes.map(([cell]) => [cell, state.values[cell], state.candidates[cell]]));
    for (const [cell, value, mask] of step.changes) {
      filled += (value ? 1 : 0) - (state.values[cell] ? 1 : 0);
      state.values[cell] = value;
      state.candidates[cell] = mask;
    }
    counts[i + 1] = filled;
  });

  const write = (target: BoardState, changes: LogicStep['changes']) => {
    for (const [cell, value, mask] of changes) {
      target.values[cell] = value;
      target.candidates[cell] = mask;
    }
    return changes.map(([cell]) => cell);
  };

  return {
    length: steps.length,
    showsCandidates: true,
    initial: start,
    forward: (target, i) => write(target, steps[i].changes),
    backward: (target, i) => write(target, previous[i]),
    view: (i) => ({ kind: 'logic', step: steps[i] }),
    filledCounts: () => counts,
  };
}

export function traceOf(response: SolveResponse): Trace {
  const puzzle = parsePuzzle(response.puzzle)!;
  return response.trace.kind === 'logic'
    ? logicTrace(puzzle, response.trace.candidates, response.trace.steps)
    : searchTrace(puzzle, response.trace.events);
}

/** A cursor into a trace: the first `position` steps are applied to `state`. */
export class TracePlayer {
  position = 0;
  readonly state: BoardState;

  constructor(readonly trace: Trace) {
    this.state = trace.initial();
  }

  get atStart() {
    return this.position === 0;
  }

  get atEnd() {
    return this.position === this.trace.length;
  }

  /** The last applied step, which the board highlights. */
  get current(): StepView | null {
    return this.position > 0 ? this.trace.view(this.position - 1) : null;
  }

  /** Moves to `target` and returns every cell that changed on the way. */
  seek(target: number): Set<number> {
    const goal = Math.max(0, Math.min(this.trace.length, Math.round(target)));
    const touched = new Set<number>();
    while (this.position < goal) {
      for (const cell of this.trace.forward(this.state, this.position)) touched.add(cell);
      this.position++;
    }
    while (this.position > goal) {
      this.position--;
      for (const cell of this.trace.backward(this.state, this.position)) touched.add(cell);
    }
    return touched;
  }
}
