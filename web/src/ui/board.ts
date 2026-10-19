import { PEERS, UNITS, bit, cellName, colOf, digitsOf, rowOf } from '../sudoku/grid';
import { h } from './dom';

/** Who wrote a digit: the puzzle, the player, the solver; or a wrong entry. */
export type Tone = 'given' | 'entry' | 'solver' | 'wrong';

export interface BoardHighlight {
  /** Houses the current deduction lives in. */
  units?: readonly number[];
  /** Cells forming the pattern. */
  cells?: readonly number[];
  /** Candidates forming the pattern. */
  focus?: readonly (readonly [number, number])[];
  /** Candidates the deduction removed, drawn struck through. */
  eliminated?: readonly (readonly [number, number])[];
  /** Cells that just received a digit. */
  placed?: readonly number[];
  /** A search event on one cell. */
  event?: { cell: number; kind: 'guess' | 'forced' | 'undo'; digit: number };
}

export interface BoardOptions {
  label: string;
  onSelect?: (cell: number) => void;
  /** Keys other than arrows, with the selected cell. */
  onKey?: (event: KeyboardEvent, cell: number) => void;
}

const HIGHLIGHT_CLASSES = ['in-unit', 'pattern', 'placed', 'event-guess', 'event-forced', 'event-undo'];

/** The 9×9 grid: digits, pencil marks, selection and step highlights. */
export class BoardView {
  readonly element: HTMLDivElement;
  /** Set false at high playback speeds, where per-step animation is just noise. */
  animate = true;
  private readonly cells: HTMLDivElement[] = [];
  private readonly digits: HTMLSpanElement[] = [];
  private readonly marks: HTMLSpanElement[][] = [];
  private readonly value = new Uint8Array(81);
  private readonly mask = new Uint16Array(81);
  private readonly tone: (Tone | '')[] = new Array(81).fill('');
  private lit: Element[] = [];
  private selection: Element[] = [];
  private selected = -1;

  constructor(private readonly options: BoardOptions) {
    this.element = h('div', { class: 'board', role: 'group', 'aria-label': options.label });
    for (let box = 0; box < 9; box++) {
      const boxElement = h('div', { class: 'box' });
      for (const cell of UNITS[18 + box]) boxElement.append(this.createCell(cell));
      this.element.append(boxElement);
    }
    this.element.addEventListener('keydown', (event) => this.onKeyDown(event));
    for (let cell = 0; cell < 81; cell++) this.updateLabel(cell);
  }

  get selectedCell() {
    return this.selected;
  }

  /** Draws one cell; unchanged parts are skipped, so calling this often is cheap. */
  render(cell: number, value: number, candidates: number, tone: Tone) {
    let changed = false;
    if (value !== this.value[cell]) {
      this.value[cell] = value;
      this.digits[cell].textContent = value ? String(value) : '';
      this.cells[cell].classList.toggle('filled', value !== 0);
      changed = true;
    }
    const marks = value ? 0 : candidates;
    if (marks !== this.mask[cell]) {
      const flipped = marks ^ this.mask[cell];
      for (let d = 1; d <= 9; d++) {
        if (flipped & bit(d)) this.marks[cell][d - 1].classList.toggle('on', (marks & bit(d)) !== 0);
      }
      this.mask[cell] = marks;
      changed = true;
    }
    if (tone !== this.tone[cell]) {
      this.tone[cell] = tone;
      this.cells[cell].dataset.tone = tone;
      changed = true;
    }
    if (changed) this.updateLabel(cell);
  }

  renderAll(values: ArrayLike<number>, candidates: ArrayLike<number>, toneOf: (cell: number) => Tone) {
    for (let cell = 0; cell < 81; cell++) this.render(cell, values[cell], candidates[cell], toneOf(cell));
  }

  /** Replaces the step highlight (units, pattern, eliminations, events). */
  highlight(highlight: BoardHighlight | null) {
    for (const element of this.lit) {
      element.classList.remove(...HIGHLIGHT_CLASSES, 'focus', 'gone');
      if (element instanceof HTMLElement) delete element.dataset.ghost;
    }
    this.lit = [];
    if (!highlight) return;

    const light = (element: Element, className: string) => {
      element.classList.add(className);
      this.lit.push(element);
    };
    for (const unit of highlight.units ?? []) {
      for (const cell of UNITS[unit]) light(this.cells[cell], 'in-unit');
    }
    for (const cell of highlight.cells ?? []) light(this.cells[cell], 'pattern');
    for (const [cell, digit] of highlight.focus ?? []) light(this.marks[cell][digit - 1], 'focus');
    for (const [cell, digit] of highlight.eliminated ?? []) light(this.marks[cell][digit - 1], 'gone');
    for (const cell of highlight.placed ?? []) {
      light(this.cells[cell], 'placed');
      this.pop(cell);
    }
    if (highlight.event) {
      const { cell, kind, digit } = highlight.event;
      light(this.cells[cell], `event-${kind}`);
      if (kind === 'undo') this.cells[cell].dataset.ghost = String(digit);
      else this.pop(cell);
    }
  }

  /** Marks the selected cell, and optionally its peers and every cell with the same digit. */
  select(cell: number, { peers = true, sameDigit = true } = {}) {
    for (const element of this.selection) element.classList.remove('selected', 'peer', 'same-digit');
    this.selection = [];
    if (this.selected >= 0) this.cells[this.selected].tabIndex = -1;
    this.selected = cell;
    if (cell < 0) {
      this.cells[0].tabIndex = 0;
      return;
    }
    const mark = (target: number, className: string) => {
      this.cells[target].classList.add(className);
      this.selection.push(this.cells[target]);
    };
    if (peers) for (const peer of PEERS[cell]) mark(peer, 'peer');
    const digit = this.value[cell];
    if (sameDigit && digit) {
      for (let other = 0; other < 81; other++) if (other !== cell && this.value[other] === digit) mark(other, 'same-digit');
    }
    mark(cell, 'selected');
    this.cells[cell].tabIndex = 0;
    // Keep keyboard focus on the selection when it moves programmatically (e.g. to a hint).
    if (this.element.contains(document.activeElement)) this.cells[cell].focus({ preventScroll: true });
  }

  focus() {
    this.cells[Math.max(this.selected, 0)].focus({ preventScroll: true });
  }

  /** Briefly flashes cells, e.g. to point at mistakes. */
  flash(cells: Iterable<number>, className = 'flash') {
    for (const cell of cells) {
      const element = this.cells[cell];
      element.classList.remove(className);
      void element.offsetWidth; // restart the CSS animation
      element.classList.add(className);
      element.addEventListener('animationend', () => element.classList.remove(className), { once: true });
    }
  }

  private pop(cell: number) {
    if (!this.animate || typeof this.digits[cell].animate !== 'function') return;
    this.digits[cell].animate(
      [
        { transform: 'scale(0.5)', opacity: 0 },
        { transform: 'scale(1.12)', opacity: 1, offset: 0.65 },
        { transform: 'scale(1)', opacity: 1 },
      ],
      { duration: 260, easing: 'cubic-bezier(.2,.8,.2,1)' },
    );
  }

  private createCell(cell: number) {
    const digit = h('span', { class: 'digit' });
    const marks = h('span', { class: 'marks', 'aria-hidden': 'true' });
    this.marks[cell] = [];
    for (let d = 1; d <= 9; d++) {
      const mark = h('span', { text: d });
      this.marks[cell].push(mark);
      marks.append(mark);
    }
    const element = h(
      'div',
      {
        class: 'cell',
        role: 'button',
        tabindex: cell === 0 ? 0 : -1,
        'data-row': rowOf(cell),
        'data-col': colOf(cell),
        onpointerdown: (event: PointerEvent) => {
          event.preventDefault();
          this.cells[cell].focus({ preventScroll: true });
          this.options.onSelect?.(cell);
        },
      },
      digit,
      marks,
    );
    this.cells[cell] = element;
    this.digits[cell] = digit;
    return element;
  }

  private updateLabel(cell: number) {
    const value = this.value[cell];
    const notes = digitsOf(this.mask[cell]);
    const content = value
      ? `${value}${this.tone[cell] === 'given' ? ', given' : this.tone[cell] === 'wrong' ? ', wrong' : ''}`
      : notes.length
        ? `empty, candidates ${notes.join(' ')}`
        : 'empty';
    this.cells[cell].setAttribute('aria-label', `${cellName(cell)}: ${content}`);
  }

  private onKeyDown(event: KeyboardEvent) {
    const moves: Record<string, number> = { ArrowUp: -9, ArrowDown: 9, ArrowLeft: -1, ArrowRight: 1 };
    const cell = this.selected < 0 ? 0 : this.selected;
    if (event.key in moves) {
      event.preventDefault();
      const delta = moves[event.key];
      const next =
        Math.abs(delta) === 1 ? rowOf(cell) * 9 + ((colOf(cell) + delta + 9) % 9) : (cell + delta + 81) % 81;
      this.options.onSelect?.(next);
      this.cells[next].focus({ preventScroll: true });
      return;
    }
    this.options.onKey?.(event, cell);
  }
}
