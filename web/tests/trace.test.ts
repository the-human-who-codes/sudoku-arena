import { describe, expect, it } from 'vitest';

import type { LogicStep } from '../src/engine/types';
import { TracePlayer, decodeEvent, logicTrace, searchTrace } from '../src/player/trace';

const encode = (kind: number, cell: number, digit: number) => (kind << 11) | (cell << 4) | digit;

describe('search traces', () => {
  const puzzle = new Array(81).fill(0);
  puzzle[80] = 9;
  // Try 1 at r1c1, take it back, try 2, then a forced 3 at r1c2.
  const events = Uint32Array.from([encode(0, 0, 1), encode(2, 0, 1), encode(0, 0, 2), encode(1, 1, 3)]);

  it('decodes packed events', () => {
    expect(decodeEvent(events[3])).toEqual({ kind: 'forced', cell: 1, digit: 3 });
  });

  it('steps forwards and backwards', () => {
    const player = new TracePlayer(searchTrace(puzzle, events));
    player.seek(1);
    expect(player.state.values[0]).toBe(1);
    player.seek(2);
    expect(player.state.values[0]).toBe(0);
    expect([...player.seek(4)].sort()).toEqual([0, 1]);
    expect([...player.state.values.slice(0, 2)]).toEqual([2, 3]);
    player.seek(1);
    expect([...player.state.values.slice(0, 2)]).toEqual([1, 0]);
    expect(player.current).toEqual({ kind: 'guess', cell: 0, digit: 1 });
  });

  it('counts filled cells after every step', () => {
    expect([...searchTrace(puzzle, events).filledCounts()]).toEqual([1, 2, 1, 2, 3]);
  });
});

describe('logic traces', () => {
  const step = (changes: LogicStep['changes']): LogicStep => ({
    technique: 'naked-single',
    name: 'Naked Single',
    level: 'easy',
    text: '',
    place: [],
    eliminate: [],
    cells: [],
    units: [],
    digits: [],
    changes,
  });
  const puzzle = new Array(81).fill(0);
  const candidates = new Array(81).fill(0b111);

  it('restores overwritten candidates when stepping back', () => {
    const trace = logicTrace(puzzle, candidates, [step([[0, 1, 0], [1, 0, 0b110]]), step([[1, 2, 0]])]);
    const player = new TracePlayer(trace);
    player.seek(2);
    expect([...player.state.values.slice(0, 2)]).toEqual([1, 2]);
    player.seek(0);
    expect([...player.state.values.slice(0, 2)]).toEqual([0, 0]);
    expect([...player.state.candidates.slice(0, 2)]).toEqual([0b111, 0b111]);
    expect([...trace.filledCounts()]).toEqual([0, 1, 2]);
  });
});
