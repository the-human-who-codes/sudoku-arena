/** 1,284 → "1.3K", 69175316 → "69.2M". */
export function compact(value: number): string {
  return new Intl.NumberFormat('en', { notation: 'compact', maximumFractionDigits: 1 }).format(value);
}

/** Engine time with a unit that keeps it readable: 1.6 µs, 4.2 ms, 1.03 s. */
export function formatMicros(micros: number, digits = 1): string {
  if (micros < 1000) return `${micros.toFixed(micros < 10 ? digits : 0)} µs`;
  if (micros < 1e6) return `${(micros / 1000).toFixed(micros < 1e4 ? digits : 0)} ms`;
  return `${(micros / 1e6).toFixed(2)} s`;
}

export const capitalize = (text: string) => text.charAt(0).toUpperCase() + text.slice(1);
