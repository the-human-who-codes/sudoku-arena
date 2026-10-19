export interface Sample {
  name: string;
  puzzle: string;
  blurb: string;
}

// Same puzzles as engine/include/sudoku/samples.hpp.
export const SAMPLES: readonly Sample[] = [
  {
    name: 'Classic',
    puzzle: '53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79',
    blurb: 'The Wikipedia example. Hidden singles and full houses solve it start to finish.',
  },
  {
    name: 'AI Escargot',
    puzzle: '1....7.9..3..2...8..96..5....53..9...1..8...26....4...3......1..4......7..7...3..',
    blurb: 'Arto Inkala, 2006: billed as the most difficult Sudoku of its time.',
  },
  {
    name: 'Inkala 2012',
    puzzle: '8..........36......7..9.2...5...7.......457.....1...3...1....68..85...1..9....4..',
    blurb: 'The puzzle newspapers called the world’s hardest Sudoku in 2012.',
  },
  {
    name: 'Easter Monster',
    puzzle: '1.......2.9.4...5...6...7...5.9.3.......7.......85..4.7.....6...3...9.8...2.....1',
    blurb: 'One of the toughest known puzzles for human-style solving.',
  },
  {
    name: 'Anti-Brute-Force',
    puzzle: '..............3.85..1.2.......5.7.....4...1...9.......5......73..2.1........4...9',
    blurb: '17 clues, built so the top row solves to 987654321: naive backtracking needs 69 million placements.',
  },
];
