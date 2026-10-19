// Grid helpers mirroring the engine's conventions: 81 cells in reading order,
// 0 for empty, units numbered 0-8 (rows), 9-17 (columns), 18-26 (boxes).

export type Values = number[];

export const rowOf = (cell: number) => Math.floor(cell / 9);
export const colOf = (cell: number) => cell % 9;
export const boxOf = (cell: number) => Math.floor(cell / 27) * 3 + Math.floor((cell % 9) / 3);

export const UNITS: readonly number[][] = Array.from({ length: 27 }, (_, unit) =>
  Array.from({ length: 9 }, (_, i) => {
    if (unit < 9) return unit * 9 + i;
    if (unit < 18) return i * 9 + (unit - 9);
    const box = unit - 18;
    return Math.floor(box / 3) * 27 + (box % 3) * 3 + Math.floor(i / 3) * 9 + (i % 3);
  }),
);

export const unitsOf = (cell: number) => [rowOf(cell), 9 + colOf(cell), 18 + boxOf(cell)];

export const PEERS: readonly number[][] = Array.from({ length: 81 }, (_, cell) => {
  const peers = new Set<number>();
  for (const unit of unitsOf(cell)) {
    for (const other of UNITS[unit]) if (other !== cell) peers.add(other);
  }
  return [...peers];
});

export const bit = (digit: number) => 1 << (digit - 1);

export function digitsOf(mask: number): number[] {
  const digits: number[] = [];
  for (let d = 1; d <= 9; d++) if (mask & bit(d)) digits.push(d);
  return digits;
}

/** Same rules as the engine: digits are givens; 0 . _ * are blanks; whitespace and |+-= are ignored. */
export function parsePuzzle(text: string): Values | null {
  const values: Values = [];
  for (const ch of text) {
    if (ch >= '1' && ch <= '9') values.push(Number(ch));
    else if ('0._*'.includes(ch)) values.push(0);
    else if (!/[\s|+\-=]/.test(ch)) return null;
    if (values.length > 81) return null;
  }
  return values.length === 81 ? values : null;
}

export const formatPuzzle = (values: Values, empty = '.') =>
  values.map((v) => (v ? String(v) : empty)).join('');

export const cellName = (cell: number) => `r${rowOf(cell) + 1}c${colOf(cell) + 1}`;

export function unitName(unit: number): string {
  const kind = ['row', 'column', 'box'][Math.floor(unit / 9)];
  return `${kind} ${(unit % 9) + 1}`;
}

/** Cells holding a digit that repeats within one of their units. */
export function conflicts(values: Values): Set<number> {
  const found = new Set<number>();
  for (const unit of UNITS) {
    for (let i = 0; i < 9; i++) {
      for (let j = i + 1; j < 9; j++) {
        const a = unit[i];
        const b = unit[j];
        if (values[a] && values[a] === values[b]) found.add(a).add(b);
      }
    }
  }
  return found;
}

/** Digits not yet used by any peer of `cell`. */
export function allowedDigits(values: Values, cell: number): number {
  let used = 0;
  for (const peer of PEERS[cell]) if (values[peer]) used |= bit(values[peer]);
  return 0x1ff & ~used;
}

export const clueCount = (values: Values) => values.filter(Boolean).length;
