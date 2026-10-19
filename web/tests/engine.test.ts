// Integration tests against the real WebAssembly build (run `npm run engine` first).
import { beforeAll, describe, expect, it } from 'vitest';

import createSudokuEngine, { type MainModule } from '../src/engine/generated/sudoku.mjs';
import type { GenerateResponse, HintResponse, SolveResponse } from '../src/engine/types';
import { TracePlayer, traceOf } from '../src/player/trace';
import { formatPuzzle } from '../src/sudoku/grid';
import { SAMPLES } from '../src/sudoku/samples';

let engine: MainModule;

beforeAll(async () => {
  engine = await createSudokuEngine();
});

const solve = (puzzle: string, algorithm: string, limit = 1_000_000): SolveResponse => {
  const response = JSON.parse(engine.solve(puzzle, algorithm, limit));
  if (response.trace?.kind === 'search') response.trace.events = Uint32Array.from(response.trace.events);
  return response;
};

describe('WebAssembly engine', () => {
  // Same seed, same puzzle as the native build (engine/tests/generator_test.cpp).
  it('generates the golden puzzle, bit for bit', () => {
    const generated: GenerateResponse = JSON.parse(engine.generate('medium', '2026'));
    expect(generated.puzzle).toBe(
      '3....5...1..8.794.....9....7.918.6....8...5....6.597.2....2.....634.1..5...5....3',
    );
  });

  it.each(['logic', 'smart', 'dlx', 'naive'])('%s traces replay to the solution', (algorithm) => {
    for (const sample of SAMPLES.slice(0, 2)) {
      const response = solve(sample.puzzle, algorithm);
      expect(response.status).toBe('solved');
      const player = new TracePlayer(traceOf(response));
      player.seek(Infinity);
      expect(formatPuzzle([...player.state.values])).toBe(response.solution);
      player.seek(0);
      expect(formatPuzzle([...player.state.values])).toBe(sample.puzzle);
    }
  });

  it('flags truncated search traces', () => {
    const response = solve(SAMPLES[1].puzzle, 'naive', 100);
    expect(response.trace.kind).toBe('search');
    if (response.trace.kind === 'search') {
      expect(response.trace.truncated).toBe(true);
      expect(response.trace.events.length).toBe(100);
    }
  });

  it('hints the next deduction', () => {
    const puzzle = SAMPLES[0].puzzle;
    const hint: HintResponse = JSON.parse(engine.hint(puzzle, puzzle));
    expect(hint.kind).toBe('step');
    if (hint.kind === 'step') expect(hint.steps.at(-1)!.place).toHaveLength(1);
  });
});
