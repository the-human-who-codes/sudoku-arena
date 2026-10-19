// Shapes of the JSON documents returned by the C++ engine (engine/src/api.cpp).

export type Algorithm = 'logic' | 'smart' | 'dlx' | 'naive';
export type Level = 'easy' | 'medium' | 'hard' | 'expert' | 'master';
export type SolveStatus = 'solved' | 'unsolvable' | 'invalid' | 'stuck' | 'limit';

export const LEVELS: readonly Level[] = ['easy', 'medium', 'hard', 'expert', 'master'];

export interface Rating {
  level: Level;
  hardest: string;
  hardestName: string;
}

export interface TechniqueCount {
  id: string;
  name: string;
  level: Level;
  count: number;
}

export type CellDigit = [cell: number, digit: number];

export interface LogicStep {
  technique: string;
  name: string;
  level: Level;
  text: string;
  place: CellDigit[];
  eliminate: CellDigit[];
  cells: number[];
  units: number[];
  digits: number[];
  /** New state of each cell the step changed: [cell, value, candidate mask]. */
  changes: [cell: number, value: number, candidates: number][];
}

interface SolveBase {
  ok: true;
  puzzle: string;
  status: SolveStatus;
  solution: string | null;
  micros: number;
}

export interface SearchSolve extends SolveBase {
  algorithm: 'smart' | 'dlx' | 'naive';
  stats: { placements: number; guesses: number; backtracks: number };
  /** Events are packed as kind << 11 | cell << 4 | digit; kind 0 guess, 1 forced, 2 undo. */
  trace: { kind: 'search'; truncated: boolean; events: Uint32Array };
}

export interface LogicSolve extends SolveBase {
  algorithm: 'logic';
  stats: { steps: number; placements: number; guesses: number; backtracks: number };
  rating?: Rating;
  techniques: TechniqueCount[];
  trace: { kind: 'logic'; candidates: number[]; steps: LogicStep[] };
}

export type SolveResponse = SearchSolve | LogicSolve;

export interface GenerateResponse {
  ok: true;
  puzzle: string;
  solution: string;
  requested: Level;
  seed: string;
  clues: number;
  attempts: number;
  micros: number;
  rating: Rating;
}

export interface AnalyzeResponse {
  ok: true;
  puzzle: string;
  clues: number;
  valid: boolean;
  solutions: number;
  conflicts: number[];
  rating?: Rating;
}

export type HintResponse = { ok: true; text: string } & (
  | { kind: 'mistakes'; cells: number[] }
  | { kind: 'step'; steps: LogicStep[] }
  | { kind: 'reveal'; cell: number; digit: number }
  | { kind: 'solved' | 'unsolvable' | 'none' }
);

export interface BenchmarkResult {
  algorithm: Algorithm;
  status: SolveStatus;
  micros: number;
  runs: number;
  placements?: number;
  steps?: number;
  guesses: number;
  backtracks: number;
}

export interface BenchmarkResponse {
  ok: true;
  puzzle: string;
  results: BenchmarkResult[];
}

export interface AlgorithmInfo {
  id: Algorithm;
  name: string;
  short: string;
  description: string;
}

export const ALGORITHMS: readonly AlgorithmInfo[] = [
  {
    id: 'logic',
    name: 'Human logic',
    short: 'Logic',
    description:
      'Solves the way a person would: always applies the easiest technique that makes progress (singles, locked candidates, subsets, fish, wings) and explains every deduction. Guesses only when every technique is exhausted.',
  },
  {
    id: 'smart',
    name: 'Constraint propagation',
    short: 'Propagation',
    description:
      'Depth-first search that fills every naked and hidden single before branching, and always branches on the cell with the fewest candidates. Usually solves in microseconds with few or no guesses.',
  },
  {
    id: 'dlx',
    name: 'Dancing Links',
    short: 'DLX',
    description:
      'Knuth’s Algorithm X on the 729 × 324 exact-cover matrix. Doubly linked lists let it cover and uncover constraints in constant time while it searches.',
  },
  {
    id: 'naive',
    name: 'Naive backtracking',
    short: 'Naive',
    description:
      'The textbook approach: fill cells in reading order, try 1–9, and back up at every dead end. Simple, but some puzzles need tens of millions of placements.',
  },
];

export const algorithmInfo = (id: Algorithm) => ALGORITHMS.find((a) => a.id === id)!;
