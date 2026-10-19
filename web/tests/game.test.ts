import { describe, expect, it } from 'vitest';

import { Game, formatClock } from '../src/game/game';
import { bit } from '../src/sudoku/grid';

const PUZZLE = '53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79';
const SOLUTION = '534678912672195348198342567859761423426853791713924856961537284287419635345286179';
const newGame = () => Game.create({ puzzle: PUZZLE, solution: SOLUTION, level: 'medium', seed: 'test' });

describe('Game', () => {
  it('counts wrong entries as mistakes and ignores givens', () => {
    const game = newGame();
    expect(game.enter(0, 9)).toBe('ignored'); // r1c1 is a given
    expect(game.enter(2, 4)).toBe('correct');
    expect(game.enter(3, 1)).toBe('wrong');
    expect(game.mistakes).toBe(1);
    expect(game.isWrong(3)).toBe(true);
    expect(game.isWrong(2)).toBe(false);
  });

  it('clears a placed digit from the notes of its peers', () => {
    const game = newGame();
    game.toggleNote(3, 6); // r1c4, same row as r1c3
    game.toggleNote(72, 6); // r9c1: shares no unit with r1c6
    game.enter(2, 4);
    game.toggleNote(3, 4);
    expect(game.notes[3]).toBe(bit(6) | bit(4));
    game.enter(5, 4, true); // r1c6 shares a row with r1c4
    expect(game.notes[3]).toBe(bit(6));
    expect(game.notes[72]).toBe(bit(6));
  });

  it('undoes and redoes whole moves', () => {
    const game = newGame();
    game.toggleNote(3, 6);
    game.enter(5, 6);
    expect(game.notes[3]).toBe(0);
    expect(game.undo().sort()).toEqual([3, 5]);
    expect(game.values[5]).toBe(0);
    expect(game.notes[3]).toBe(bit(6));
    game.redo();
    expect(game.values[5]).toBe(6);
    expect(game.notes[3]).toBe(0);
    expect(game.canRedo).toBe(false);
  });

  it('erases several cells as one move', () => {
    const game = newGame();
    game.enter(2, 1);
    game.enter(3, 2);
    expect(game.erase(2, 3, 0)).toBe(true); // the given at 0 is left alone
    expect(game.values.slice(0, 4)).toEqual([5, 3, 0, 0]);
    game.undo();
    expect(game.values.slice(2, 4)).toEqual([1, 2]);
  });

  it('fills notes with every allowed digit', () => {
    const game = newGame();
    game.fillNotes();
    expect(game.notes[2]).toBe(bit(1) | bit(2) | bit(4));
    expect(game.notes[0]).toBe(0);
  });

  it('completes once every cell matches the solution', () => {
    const game = newGame();
    for (let cell = 0; cell < 81; cell++) {
      if (!game.isGiven(cell)) game.enter(cell, Number(SOLUTION[cell]));
    }
    expect(game.completed).toBe(true);
    expect(game.progress()).toBe(1);
    expect(game.enter(2, 9)).toBe('ignored');
  });

  it('survives a save and restore', () => {
    const game = newGame();
    game.enter(2, 4);
    game.toggleNote(3, 6);
    game.elapsedMs = 61_000;
    const copy = Game.restore(JSON.parse(JSON.stringify(game.toJSON())));
    expect(copy.values).toEqual(game.values);
    expect(copy.notes).toEqual(game.notes);
    expect(copy.elapsedMs).toBe(61_000);
    copy.undo();
    expect(copy.notes[3]).toBe(0);
  });
});

describe('formatClock', () => {
  it('formats minutes and hours', () => {
    expect(formatClock(0)).toBe('0:00');
    expect(formatClock(272_000)).toBe('4:32');
    expect(formatClock(3_725_000)).toBe('1:02:05');
  });
});
