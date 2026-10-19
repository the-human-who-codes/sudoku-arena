import type { Level } from '../engine/types';
import { PEERS, allowedDigits, bit, parsePuzzle, type Values } from '../sudoku/grid';

export interface GameSetup {
  puzzle: string;
  solution: string;
  level: Level;
  seed: string;
  /** Set when playing someone else's challenge link: their time to beat. */
  challengeSeconds?: number;
}

interface CellSnapshot {
  cell: number;
  value: number;
  notes: number;
}

/** The cells a move overwrote, as they were before it. */
type Move = CellSnapshot[];

export interface GameData {
  setup: GameSetup;
  values: Values;
  notes: number[];
  mistakes: number;
  hints: number;
  elapsedMs: number;
  history: Move[];
}

export type EntryResult = 'correct' | 'wrong' | 'ignored';

/** One game of Sudoku: entries, notes, undo history, mistakes and the clock. */
export class Game {
  readonly puzzle: Values;
  readonly solution: Values;
  values: Values;
  notes: number[];
  mistakes = 0;
  hints = 0;
  elapsedMs = 0;
  private history: Move[] = [];
  private future: Move[] = [];

  private constructor(readonly setup: GameSetup) {
    this.puzzle = parsePuzzle(setup.puzzle)!;
    this.solution = parsePuzzle(setup.solution)!;
    this.values = [...this.puzzle];
    this.notes = new Array(81).fill(0);
  }

  static create(setup: GameSetup) {
    return new Game(setup);
  }

  static restore(data: GameData) {
    const game = new Game(data.setup);
    game.values = [...data.values];
    game.notes = [...data.notes];
    game.mistakes = data.mistakes;
    game.hints = data.hints;
    game.elapsedMs = data.elapsedMs;
    game.history = data.history.map((move) => move.map((s) => ({ ...s })));
    return game;
  }

  toJSON(): GameData {
    return {
      setup: this.setup,
      values: this.values,
      notes: this.notes,
      mistakes: this.mistakes,
      hints: this.hints,
      elapsedMs: this.elapsedMs,
      history: this.history,
    };
  }

  get completed() {
    return this.values.every((value, cell) => value === this.solution[cell]);
  }

  get canUndo() {
    return this.history.length > 0;
  }

  get canRedo() {
    return this.future.length > 0;
  }

  isGiven(cell: number) {
    return this.puzzle[cell] !== 0;
  }

  isWrong(cell: number) {
    return this.values[cell] !== 0 && this.values[cell] !== this.solution[cell];
  }

  /** Copies of `digit` still to place (counting wrong entries as placed). */
  remaining(digit: number) {
    return 9 - this.values.filter((value) => value === digit).length;
  }

  /** Cells filled correctly, as a fraction of those that started empty. */
  progress() {
    let empty = 0;
    let done = 0;
    for (let cell = 0; cell < 81; cell++) {
      if (this.isGiven(cell)) continue;
      empty++;
      if (this.values[cell] === this.solution[cell]) done++;
    }
    return empty === 0 ? 1 : done / empty;
  }

  /** Writes a digit and, if `clearPeerNotes`, drops it from the peers' notes. */
  enter(cell: number, digit: number, clearPeerNotes = true): EntryResult {
    if (this.completed || this.isGiven(cell) || this.values[cell] === digit) return 'ignored';
    const peers = clearPeerNotes ? PEERS[cell].filter((p) => this.notes[p] & bit(digit)) : [];
    this.record([cell, ...peers]);
    this.values[cell] = digit;
    this.notes[cell] = 0;
    for (const peer of peers) this.notes[peer] &= ~bit(digit);
    const correct = digit === this.solution[cell];
    if (!correct) this.mistakes++;
    return correct ? 'correct' : 'wrong';
  }

  toggleNote(cell: number, digit: number) {
    if (this.completed || this.values[cell] !== 0) return false;
    this.record([cell]);
    this.notes[cell] ^= bit(digit);
    return true;
  }

  /** Clears entries and notes from the given cells as a single move. */
  erase(...cells: number[]) {
    const targets = cells.filter((c) => !this.isGiven(c) && (this.values[c] || this.notes[c]));
    if (this.completed || targets.length === 0) return false;
    this.record(targets);
    for (const cell of targets) {
      this.values[cell] = 0;
      this.notes[cell] = 0;
    }
    return true;
  }

  /** Pencils in every digit each empty cell still allows. */
  fillNotes() {
    const empty = this.values.flatMap((value, cell) => (value ? [] : [cell]));
    if (this.completed || empty.length === 0) return false;
    this.record(empty);
    for (const cell of empty) this.notes[cell] = allowedDigits(this.values, cell);
    return true;
  }

  /** Reverts the last move and returns the cells it touched. */
  undo(): number[] {
    return this.swap(this.history, this.future);
  }

  redo(): number[] {
    return this.swap(this.future, this.history);
  }

  private record(cells: number[]) {
    this.history.push(this.snapshot(cells));
    this.future = [];
  }

  private snapshot(cells: number[]): Move {
    return cells.map((cell) => ({ cell, value: this.values[cell], notes: this.notes[cell] }));
  }

  private swap(from: Move[], to: Move[]): number[] {
    const move = from.pop();
    if (!move) return [];
    to.push(this.snapshot(move.map((s) => s.cell)));
    for (const { cell, value, notes } of move) {
      this.values[cell] = value;
      this.notes[cell] = notes;
    }
    return move.map((s) => s.cell);
  }
}

export const formatClock = (ms: number) => {
  const total = Math.floor(ms / 1000);
  const hours = Math.floor(total / 3600);
  const minutes = Math.floor((total % 3600) / 60);
  const seconds = String(total % 60).padStart(2, '0');
  return hours > 0 ? `${hours}:${String(minutes).padStart(2, '0')}:${seconds}` : `${minutes}:${seconds}`;
};
