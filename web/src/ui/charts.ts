import { ALGORITHMS, type BenchmarkResult } from '../engine/types';
import { h, svg } from './dom';
import { compact, formatMicros } from './format';

const HEIGHT = 136;
const PAD = { left: 30, right: 10, top: 10, bottom: 22 };

/**
 * "Cells filled over time": one series, so no legend; the card title names it.
 * Dense traces are drawn as a per-pixel min/max envelope, so oscillating
 * backtracking stays honest at any zoom. Hover shows a crosshair; click seeks.
 */
export class ProfileChart {
  readonly element: HTMLElement;
  /** Called with a step position when the reader clicks the chart. */
  onSeek: ((position: number) => void) | null = null;
  private readonly plot: SVGSVGElement;
  private readonly tooltip: HTMLDivElement;
  private counts: Uint8Array | null = null;
  private floor = 0;
  private position = 0;
  private width = 0;
  private marker: { line: SVGLineElement; dot: SVGCircleElement } | null = null;
  private crosshair: SVGLineElement | null = null;

  constructor() {
    this.plot = svg('svg', { class: 'profile-plot', height: HEIGHT, role: 'img' });
    this.tooltip = h('div', { class: 'chart-tooltip', hidden: true });
    this.element = h('figure', { class: 'profile' }, this.plot, this.tooltip);
    new ResizeObserver(([entry]) => {
      const width = Math.round(entry.contentRect.width);
      if (width !== this.width) {
        this.width = width;
        this.draw();
      }
    }).observe(this.element);
    this.plot.addEventListener('pointermove', (event) => this.hover(event));
    this.plot.addEventListener('pointerleave', () => this.hover(null));
    this.plot.addEventListener('click', (event) => {
      const index = this.indexAt(event);
      if (index !== null) this.onSeek?.(index);
    });
  }

  /** `counts[i]` is the number of filled cells after `i` steps. */
  setData(counts: Uint8Array | null) {
    this.counts = counts;
    this.floor = counts ? counts[0] : 0; // the givens: a solver only ever adds to them
    this.position = 0;
    this.draw();
  }

  setPosition(position: number) {
    this.position = position;
    this.placeMarker();
  }

  private get steps() {
    return this.counts ? this.counts.length - 1 : 0;
  }

  private x(index: number) {
    const span = Math.max(this.steps, 1);
    return PAD.left + (index / span) * (this.width - PAD.left - PAD.right);
  }

  private y(value: number) {
    const range = Math.max(81 - this.floor, 1);
    return PAD.top + ((81 - value) / range) * (HEIGHT - PAD.top - PAD.bottom);
  }

  private draw() {
    this.plot.replaceChildren();
    this.marker = null;
    this.crosshair = null;
    const counts = this.counts;
    if (!counts || this.width < 80) return;
    this.plot.setAttribute('viewBox', `0 0 ${this.width} ${HEIGHT}`);
    this.plot.setAttribute('width', String(this.width));
    this.plot.setAttribute(
      'aria-label',
      `Cells filled over ${compact(this.steps)} steps, from ${counts[0]} to ${counts[counts.length - 1]}`,
    );

    const left = PAD.left;
    const right = this.width - PAD.right;
    const bottom = HEIGHT - PAD.bottom;
    // Recessive chrome: a hairline at "solved" (81) and the baseline at the clue count.
    this.plot.append(
      svg('line', { class: 'grid', x1: left, x2: right, y1: this.y(81), y2: this.y(81) }),
      svg('line', { class: 'axis', x1: left, x2: right, y1: bottom, y2: bottom }),
      svg('text', { class: 'tick', x: left - 6, y: this.y(81) + 4, 'text-anchor': 'end' }),
      svg('text', { class: 'tick', x: left - 6, y: bottom + 4, 'text-anchor': 'end' }),
      svg('text', { class: 'tick', x: left, y: HEIGHT - 6 }),
      svg('text', { class: 'tick', x: right, y: HEIGHT - 6, 'text-anchor': 'end' }),
    );
    const ticks = this.plot.querySelectorAll('text');
    ticks[0].textContent = '81';
    ticks[1].textContent = String(this.floor);
    ticks[2].textContent = '0';
    ticks[3].textContent = `${compact(this.steps)} steps`;

    // Envelope: per pixel column, the lowest and highest fill level reached.
    const columns = Math.max(1, Math.floor(right - left));
    const top: string[] = [];
    const low: string[] = [];
    for (let c = 0; c <= columns; c++) {
      const from = Math.floor((c / columns) * this.steps);
      const to = Math.max(from, Math.floor(((c + 1) / columns) * this.steps) - 1);
      let min = 81;
      let max = 0;
      for (let i = from; i <= Math.min(to, this.steps); i++) {
        if (counts[i] < min) min = counts[i];
        if (counts[i] > max) max = counts[i];
      }
      const px = (left + c).toFixed(1);
      top.push(`${px},${this.y(max).toFixed(1)}`);
      low.push(`${px},${this.y(min).toFixed(1)}`);
    }
    const band = `M${top.join('L')}L${[...low].reverse().join('L')}Z`;
    const wash = `M${low.join('L')}L${right},${bottom}L${left},${bottom}Z`;
    this.plot.append(
      svg('path', { class: 'series-wash', d: wash }),
      svg('path', { class: 'series-band', d: band }),
      svg('path', { class: 'series-line', d: `M${top.join('L')}` }),
    );

    this.crosshair = svg('line', { class: 'crosshair', y1: PAD.top, y2: bottom, visibility: 'hidden' });
    const line = svg('line', { class: 'marker-line', y1: PAD.top, y2: bottom });
    const dot = svg('circle', { class: 'marker-dot', r: 4 });
    this.marker = { line, dot };
    this.plot.append(this.crosshair, line, dot);
    this.placeMarker();
  }

  private placeMarker() {
    if (!this.marker || !this.counts) return;
    const x = this.x(this.position);
    this.marker.line.setAttribute('x1', String(x));
    this.marker.line.setAttribute('x2', String(x));
    this.marker.dot.setAttribute('cx', String(x));
    this.marker.dot.setAttribute('cy', String(this.y(this.counts[this.position])));
  }

  private indexAt(event: MouseEvent): number | null {
    if (!this.counts) return null;
    const box = this.plot.getBoundingClientRect();
    const fraction = (event.clientX - box.left - PAD.left) / (box.width - PAD.left - PAD.right);
    if (fraction < -0.02 || fraction > 1.02) return null;
    return Math.round(Math.max(0, Math.min(1, fraction)) * this.steps);
  }

  private hover(event: PointerEvent | null) {
    const index = event ? this.indexAt(event) : null;
    if (index === null || !this.counts || !this.crosshair) {
      this.tooltip.hidden = true;
      this.crosshair?.setAttribute('visibility', 'hidden');
      return;
    }
    const x = this.x(index);
    this.crosshair.setAttribute('x1', String(x));
    this.crosshair.setAttribute('x2', String(x));
    this.crosshair.setAttribute('visibility', 'visible');
    this.tooltip.replaceChildren(
      h('strong', { text: `${this.counts[index]} of 81 filled` }),
      h('span', { text: `step ${index.toLocaleString()}` }),
    );
    this.tooltip.hidden = false;
    const flip = x > this.width * 0.66;
    this.tooltip.style.left = `${flip ? x - 8 : x + 8}px`;
    this.tooltip.style.transform = flip ? 'translateX(-100%)' : '';
  }
}

// 10^p microseconds as a round label: 100 ns, 1 µs, 10 ms, 1 s.
function decadeLabel(p: number): string {
  const units = ['ns', 'µs', 'ms', 's'];
  const unit = Math.min(Math.max(Math.floor(p / 3) + 1, 0), units.length - 1);
  return `${10 ** (p - (unit - 1) * 3)} ${units[unit]}`;
}

/**
 * Engine race: one dot per algorithm on a log time axis (a Cleveland dot plot:
 * bars would imply a meaningful zero, which a log scale doesn't have). Each row
 * also states its value in text, so nothing depends on reading the dot.
 */
export function raceChart(results: readonly BenchmarkResult[]): HTMLElement {
  const solved = results.filter((r) => r.status === 'solved');
  if (solved.length === 0) {
    return h('p', { class: 'note', text: 'None of the engines could solve this puzzle: it has no solution or repeats a given.' });
  }
  const times = solved.map((r) => r.micros);
  const fastest = Math.min(...times);
  const lo = Math.floor(Math.log10(Math.max(fastest, 0.1)));
  const hi = Math.max(lo + 2, Math.ceil(Math.log10(Math.max(...times))));
  const at = (micros: number) => ((Math.log10(micros) - lo) / (hi - lo)) * 100;

  const rows = results.map((result) => {
    const info = ALGORITHMS.find((a) => a.id === result.algorithm)!;
    const ok = result.status === 'solved';
    const work =
      result.steps !== undefined
        ? `${compact(result.steps)} steps`
        : `${compact(result.placements ?? 0)} placements`;
    const ratio = ok && result.micros > fastest * 1.05 ? `${compact(result.micros / fastest)}× slower` : ok ? 'fastest' : '';
    const track = h('div', { class: 'race-track' });
    if (ok) track.append(h('span', { class: 'race-dot', style: `left:${at(result.micros)}%` }));
    return h(
      'div',
      {
        class: 'race-row',
        tabindex: 0,
        title: `${info.name}: ${formatMicros(result.micros)} per solve, averaged over ${result.runs} run${result.runs === 1 ? '' : 's'}; ${work}, ${compact(result.guesses)} guesses, ${compact(result.backtracks)} backtracks`,
      },
      h('span', { class: 'race-name', text: info.short }),
      track,
      h(
        'span',
        { class: 'race-value' },
        h('strong', { text: ok ? formatMicros(result.micros) : 'gave up' }),
        h('span', { text: ratio || work }),
      ),
    );
  });

  const axis = h('div', { class: 'race-axis', 'aria-hidden': 'true' });
  for (let p = lo; p <= hi; p++) {
    axis.append(h('span', { style: `left:${((p - lo) / (hi - lo)) * 100}%`, text: decadeLabel(p) }));
  }
  return h('div', { class: 'race' }, ...rows, h('div', { class: 'race-axis-row' }, h('span'), axis, h('span')));
}
