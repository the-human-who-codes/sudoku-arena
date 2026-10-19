import { CancelledError, engine } from '../engine/client';
import { LEVELS, type HintResponse, type Level } from '../engine/types';
import { Game, formatClock } from '../game/game';
import { loadGame, loadSettings, saveGame, saveSettings, type Settings } from '../game/storage';
import { bit, formatPuzzle } from '../sudoku/grid';
import { BoardView, type BoardHighlight } from '../ui/board';
import { ICONS, h, icon } from '../ui/dom';
import { capitalize } from '../ui/format';
import { copyText, toast } from '../ui/toast';

/** Everyone gets the same daily puzzle: the seed is the UTC date. */
export const dailySeed = (date = new Date()) => `daily-${date.toISOString().slice(0, 10)}`;
const DAILY_LEVEL: Level = 'hard';

const randomSeed = () => Math.floor(Math.random() * 2 ** 32).toString(36);

export function challengeLink(level: Level, seed: string, seconds?: number) {
  const url = new URL(window.location.href);
  url.hash = `/play?level=${level}&seed=${encodeURIComponent(seed)}${seconds ? `&vs=${seconds}` : ''}`;
  return url.toString();
}

export class PlayView {
  readonly element: HTMLElement;
  private game: Game | null = null;
  private selected = -1;
  private notesMode = false;
  private settings: Settings = loadSettings();
  private level: Level = 'medium';
  private hint: HintResponse | null = null;
  private generating = false;
  private visible = false;
  private clockTimer = 0;
  private lastTick = 0;
  private lastSave = 0;

  private readonly board: BoardView;
  private readonly levelName = h('span', { class: 'level-name' });
  private readonly clock = h('span', { class: 'clock', text: '0:00', 'aria-label': 'Time' });
  private readonly mistakes = h('span');
  private readonly hintsUsed = h('span');
  private readonly banner = h('div', { class: 'banner', hidden: true });
  private readonly digitButtons: HTMLButtonElement[] = [];
  private readonly numpad = h('div', { class: 'numpad', role: 'group', 'aria-label': 'Digits' });
  private readonly notesButton = h('button', { class: 'button', 'aria-pressed': 'false' });
  private readonly undoButton = h('button', { class: 'button' });
  private readonly eraseButton = h('button', { class: 'button' });
  private readonly hintButton = h('button', { class: 'button' });
  private readonly levelButtons = new Map<Level, HTMLButtonElement>();
  private readonly seedLine = h('p', { class: 'muted small' });
  private readonly hintCard = h('section', { class: 'card', hidden: true, 'aria-live': 'polite' });
  private readonly controlsCard = h('section', { class: 'card' });
  private readonly dialog = h('dialog', { class: 'win' });

  constructor() {
    // Keys reach onKeyDown through the app-wide listener; the board handles arrows itself.
    this.board = new BoardView({ label: 'Sudoku board', onSelect: (cell) => this.select(cell) });
    this.element = h(
      'section',
      { class: 'view play' },
      h(
        'header',
        { class: 'view-head' },
        h('h1', { text: 'Play' }),
        h('p', {
          class: 'lede',
          text: 'Fresh puzzles from the same C++ generator, each with exactly one solution and graded by the techniques it needs. Stuck? Hints explain the next deduction instead of just giving the answer.',
        }),
      ),
      h('div', { class: 'split' }, this.buildStage(), this.buildPanel()),
      this.dialog,
    );
    document.addEventListener('visibilitychange', () => (this.lastTick = performance.now()));

    // On narrow screens the panel sits below the board, so keep hints next to the board.
    const narrow = window.matchMedia('(max-width: 940px)');
    const placeHint = () => (narrow.matches ? this.board.element.after(this.hintCard) : this.controlsCard.after(this.hintCard));
    narrow.addEventListener('change', placeHint);
    placeHint();
  }

  show(params: URLSearchParams) {
    this.visible = true;
    this.lastTick = performance.now();
    clearInterval(this.clockTimer);
    this.clockTimer = window.setInterval(() => this.tick(), 500);

    const level = params.get('level') as Level | null;
    const seed = params.get('seed');
    if (seed && level && LEVELS.includes(level)) {
      const vs = Number(params.get('vs')) || undefined;
      const same = this.game?.setup.seed === seed && this.game.setup.level === level;
      if (!same) void this.start(level, seed, vs);
      return;
    }
    if (this.game || this.generating) return;
    const saved = loadGame();
    if (saved) {
      this.game = Game.restore(saved);
      this.level = this.game.setup.level;
      this.selected = this.firstEmpty();
      this.renderAll();
    } else {
      void this.start('medium', randomSeed());
    }
  }

  hide() {
    this.visible = false;
    clearInterval(this.clockTimer);
    this.persist();
  }

  onKeyDown(event: KeyboardEvent) {
    if (event.defaultPrevented || !this.game) return;
    const mod = event.metaKey || event.ctrlKey;
    const key = event.key.toLowerCase();
    if (mod && key === 'z') {
      event.preventDefault();
      if (event.shiftKey) this.redo();
      else this.undo();
      return;
    }
    if (mod && key === 'y') {
      event.preventDefault();
      this.redo();
      return;
    }
    if (mod || event.altKey) return;
    if (/^[1-9]$/.test(event.key)) {
      event.preventDefault();
      this.input(Number(event.key), event.shiftKey);
    } else if (/^Digit[1-9]$/.test(event.code) && event.shiftKey) {
      event.preventDefault(); // shift+digit types a symbol; read the physical key
      this.input(Number(event.code.slice(5)), true);
    } else if (['Backspace', 'Delete', '0'].includes(event.key)) {
      event.preventDefault();
      this.erase();
    } else if (key === 'n') {
      event.preventDefault();
      this.setNotesMode(!this.notesMode);
    } else if (key === 'h') {
      event.preventDefault();
      void this.requestHint();
    }
  }

  // ---- Layout -----------------------------------------------------------------

  private buildStage() {
    return h(
      'div',
      { class: 'stage' },
      h(
        'div',
        { class: 'play-bar' },
        this.levelName,
        h('span', { class: 'meta' }, this.mistakes, this.hintsUsed, this.clock),
      ),
      this.board.element,
      this.banner,
    );
  }

  private buildPanel() {
    for (let digit = 1; digit <= 9; digit++) {
      const button = h(
        'button',
        { 'aria-label': `Enter ${digit}`, onclick: () => this.input(digit) },
        h('span', { class: 'n', text: digit }),
        h('span', { class: 'left' }),
      );
      this.digitButtons.push(button);
      this.numpad.append(button);
    }
    const tool = (button: HTMLButtonElement, paths: string, label: string, action: () => void) => {
      button.append(icon(paths), h('span', { text: label }));
      button.addEventListener('click', action);
      return button;
    };

    const segmented = h('div', { class: 'segmented', role: 'radiogroup', 'aria-label': 'Difficulty' });
    for (const level of LEVELS) {
      const button = h('button', {
        role: 'radio',
        'aria-checked': 'false',
        text: capitalize(level),
        onclick: () => {
          this.level = level;
          this.renderLevelPicker();
        },
      });
      this.levelButtons.set(level, button);
      segmented.append(button);
    }

    const switchRow = (label: string, key: keyof Settings) => {
      const input = h('input', { type: 'checkbox', role: 'switch' });
      input.checked = this.settings[key];
      input.addEventListener('change', () => {
        this.settings = { ...this.settings, [key]: input.checked };
        saveSettings(this.settings);
        this.renderAll();
      });
      return h('label', { class: 'switch' }, h('span', { text: label }), input);
    };

    this.controlsCard.append(
      this.numpad,
      h(
        'div',
        { class: 'tools' },
        tool(this.undoButton, ICONS.undo, 'Undo', () => this.undo()),
        tool(this.eraseButton, ICONS.erase, 'Erase', () => this.erase()),
        tool(this.notesButton, ICONS.pencil, 'Notes', () => this.setNotesMode(!this.notesMode)),
        tool(this.hintButton, ICONS.bulb, 'Hint', () => void this.requestHint()),
      ),
      h('p', {
        class: 'muted small keys-help',
        style: 'margin-top: 12px',
        text: 'Keys: 1–9 to enter (Shift for a note), N notes mode, H hint, Backspace erase, arrows to move, ⌘/Ctrl+Z undo.',
      }),
    );
    return h(
      'aside',
      { class: 'panel' },
      this.controlsCard,
      this.hintCard,
      h(
        'section',
        { class: 'card' },
        h('h2', { text: 'New game' }),
        segmented,
        h(
          'div',
          { class: 'row' },
          h('button', { class: 'button primary', onclick: () => void this.start(this.level, randomSeed()) }, icon(ICONS.sparkles), 'New puzzle'),
          h('button', { class: 'button', onclick: () => void this.start(DAILY_LEVEL, dailySeed()) }, icon(ICONS.calendar), 'Daily puzzle'),
          h('button', { class: 'button ghost', onclick: () => this.copyChallenge() }, icon(ICONS.link), 'Challenge a friend'),
        ),
        this.seedLine,
      ),
      h(
        'section',
        { class: 'card' },
        h('h2', { text: 'Settings' }),
        switchRow('Show mistakes', 'showMistakes'),
        switchRow('Highlight row, column and box', 'highlightPeers'),
        switchRow('Remove notes when a digit is placed', 'clearNotes'),
        h('div', { class: 'row' }, h('button', { class: 'button ghost', text: 'Fill in all notes', onclick: () => this.fillNotes() })),
      ),
    );
  }

  // ---- Game flow ----------------------------------------------------------------

  private async start(level: Level, seed: string, challengeSeconds?: number) {
    this.generating = true;
    this.board.element.classList.add('loading');
    try {
      const generated = await engine.generate(level, seed);
      this.game = Game.create({
        puzzle: generated.puzzle,
        solution: generated.solution,
        level: generated.rating.level,
        seed,
        challengeSeconds,
      });
      this.level = generated.rating.level;
      this.hint = null;
      this.selected = this.firstEmpty();
      this.lastTick = performance.now();
      if (this.dialog.open) this.dialog.close();
      const url = new URL(window.location.href);
      url.hash = `/play?level=${level}&seed=${encodeURIComponent(seed)}${challengeSeconds ? `&vs=${challengeSeconds}` : ''}`;
      history.replaceState(null, '', url);
      this.persist();
      this.renderAll();
      this.board.focus();
    } catch (error) {
      if (!(error instanceof CancelledError)) toast(String(error));
    } finally {
      this.generating = false;
      this.board.element.classList.remove('loading');
    }
  }

  private select(cell: number) {
    this.selected = cell;
    this.renderSelection();
  }

  /** A digit from the keyboard or keypad: a note in notes mode (Shift flips the mode), else an entry. */
  private input(digit: number, flipMode = false) {
    if (this.notesMode !== flipMode) {
      if (this.game?.toggleNote(this.selected, digit)) this.afterMove();
    } else {
      this.place(digit);
    }
  }

  private place(digit: number) {
    const game = this.game;
    if (!game || this.selected < 0 || this.generating) return;
    const result = game.enter(this.selected, digit, this.settings.clearNotes);
    if (result === 'ignored') return;
    if (result === 'wrong' && this.settings.showMistakes) this.board.flash([this.selected]);
    this.afterMove();
    if (game.completed) this.finish();
  }

  private erase() {
    if (this.game?.erase(this.selected)) this.afterMove();
  }

  private undo() {
    const cells = this.game?.undo() ?? [];
    if (cells.length) this.afterMove();
  }

  private redo() {
    const cells = this.game?.redo() ?? [];
    if (cells.length) this.afterMove();
  }

  private fillNotes() {
    if (this.game?.fillNotes()) this.afterMove();
  }

  private setNotesMode(on: boolean) {
    this.notesMode = on;
    this.notesButton.setAttribute('aria-pressed', String(on));
    this.numpad.classList.toggle('notes', on);
  }

  private afterMove() {
    this.hint = null;
    this.persist();
    this.renderAll();
  }

  private async requestHint() {
    const game = this.game;
    if (!game || game.completed || this.generating) return;
    game.hints++;
    try {
      this.hint = await engine.hint(game.setup.puzzle, formatPuzzle(game.values));
      if (this.hint.kind === 'mistakes') this.board.flash(this.hint.cells);
      const target = this.hintTarget();
      if (target) this.selected = target[0];
    } catch (error) {
      if (!(error instanceof CancelledError)) toast(String(error));
    }
    this.persist();
    this.renderAll();
  }

  /** The cell and digit the current hint leads to, if any. */
  private hintTarget(): [number, number] | null {
    const hint = this.hint;
    if (hint?.kind === 'step') return hint.steps[hint.steps.length - 1].place[0] ?? null;
    if (hint?.kind === 'reveal') return [hint.cell, hint.digit];
    return null;
  }

  private finish() {
    const game = this.game!;
    this.persist();
    const seconds = Math.max(1, Math.round(game.elapsedMs / 1000));
    const target = game.setup.challengeSeconds;
    const verdict = target
      ? seconds <= target
        ? `You beat the challenge by ${formatClock((target - seconds) * 1000)}.`
        : `The time to beat was ${formatClock(target * 1000)}. So close.`
      : 'Send this exact puzzle to a friend and see who is faster.';
    this.dialog.replaceChildren(
      h('button', { class: 'button ghost icon-only dialog-close', 'aria-label': 'Close', onclick: () => this.dialog.close() }, icon(ICONS.close)),
      h('h2', {}, icon(ICONS.trophy), ' Solved'),
      h('div', { class: 'hero', text: formatClock(game.elapsedMs) }),
      h(
        'div',
        { class: 'facts' },
        h('span', { text: capitalize(game.setup.level) }),
        h('span', { text: `${game.mistakes} mistake${game.mistakes === 1 ? '' : 's'}` }),
        h('span', { text: `${game.hints} hint${game.hints === 1 ? '' : 's'}` }),
      ),
      h('p', { class: 'muted', style: 'margin: 0 0 18px', text: verdict }),
      h(
        'div',
        { class: 'row' },
        h('button', {
          class: 'button primary',
          onclick: () => copyText(challengeLink(game.setup.level, game.setup.seed, seconds), 'Challenge link copied'),
        }, icon(ICONS.swords), 'Copy challenge link'),
        h('button', { class: 'button', text: 'New puzzle', onclick: () => void this.start(game.setup.level, randomSeed()) }),
      ),
    );
    this.dialog.showModal();
  }

  private copyChallenge() {
    const game = this.game;
    if (!game) return;
    const seconds = game.completed ? Math.round(game.elapsedMs / 1000) : undefined;
    copyText(challengeLink(game.setup.level, game.setup.seed, seconds), 'Challenge link copied: same puzzle, for anyone who opens it');
  }

  private tick() {
    const now = performance.now();
    const game = this.game;
    const running = this.visible && document.visibilityState === 'visible' && !!game && !game.completed && !this.generating;
    if (running) {
      game!.elapsedMs += now - this.lastTick;
      this.clock.textContent = formatClock(game!.elapsedMs);
      if (now - this.lastSave > 5000) this.persist();
    }
    this.lastTick = now;
  }

  private persist() {
    if (!this.game) return;
    saveGame(this.game.toJSON());
    this.lastSave = performance.now();
  }

  private firstEmpty() {
    return this.game ? Math.max(0, this.game.values.findIndex((v) => v === 0)) : 0;
  }

  // ---- Rendering ----------------------------------------------------------------

  private renderAll() {
    const game = this.game;
    if (!game) return;
    for (let cell = 0; cell < 81; cell++) {
      const tone = game.isGiven(cell) ? 'given' : this.settings.showMistakes && game.isWrong(cell) ? 'wrong' : 'entry';
      this.board.render(cell, game.values[cell], game.notes[cell], tone);
    }
    this.renderSelection();
    this.board.highlight(this.hintHighlight());

    this.levelName.textContent = game.setup.level;
    this.clock.textContent = formatClock(game.elapsedMs);
    this.mistakes.textContent = `${game.mistakes} mistake${game.mistakes === 1 ? '' : 's'}`;
    this.hintsUsed.textContent = `${game.hints} hint${game.hints === 1 ? '' : 's'}`;
    this.digitButtons.forEach((button, i) => {
      const digit = i + 1;
      const left = 9 - game.values.filter((v, cell) => v === digit && v === game.solution[cell]).length;
      button.disabled = left === 0 || game.completed;
      (button.lastElementChild as HTMLElement).textContent = left ? String(left) : '';
    });
    this.undoButton.disabled = !game.canUndo || game.completed;
    this.eraseButton.disabled = game.completed;
    this.hintButton.disabled = game.completed;
    const challenge = game.setup.challengeSeconds;
    this.banner.hidden = !challenge;
    if (challenge) this.banner.replaceChildren(icon(ICONS.swords), `Challenge: solve this puzzle in under ${formatClock(challenge * 1000)}.`);
    this.seedLine.textContent = `Seed “${game.setup.seed}”. Anyone with the same seed and level gets this exact puzzle.`;
    this.renderLevelPicker();
    this.renderHint();
  }

  private renderSelection() {
    this.board.select(this.selected, { peers: this.settings.highlightPeers, sameDigit: true });
  }

  private renderLevelPicker() {
    for (const [level, button] of this.levelButtons) button.setAttribute('aria-checked', String(level === this.level));
  }

  private hintHighlight(): BoardHighlight | null {
    const hint = this.hint;
    if (!hint || !this.game) return null;
    if (hint.kind === 'step') {
      const step = hint.steps[hint.steps.length - 1];
      const notes = this.game.notes;
      const focus: [number, number][] = [];
      for (const cell of step.cells) for (const d of step.digits) if (notes[cell] & bit(d)) focus.push([cell, d]);
      return { units: step.units, cells: step.cells, focus };
    }
    if (hint.kind === 'reveal') return { cells: [hint.cell] };
    return null;
  }

  private renderHint() {
    const hint = this.hint;
    this.hintCard.hidden = !hint;
    if (!hint) return;
    const target = this.hintTarget();
    const children: HTMLElement[] = [h('h2', {}, icon(ICONS.bulb), 'Hint'), h('p', { class: 'hint-text', text: hint.text })];
    if (hint.kind === 'step' && hint.steps.length > 1) {
      children.push(
        h('p', { class: 'muted small', style: 'margin-top: 10px', text: 'It follows from these earlier deductions:' }),
        h('ol', { class: 'hint-steps' }, ...hint.steps.slice(0, -1).map((step) => h('li', { text: step.text }))),
      );
    }
    const actions = h('div', { class: 'row', style: 'margin-top: 12px' });
    if (target) {
      const [cell, digit] = target;
      actions.append(
        h('button', {
          class: 'button primary',
          text: `Place ${digit}`,
          onclick: () => {
            this.selected = cell;
            this.place(digit);
          },
        }),
      );
    }
    if (hint.kind === 'mistakes') {
      actions.append(
        h('button', {
          class: 'button primary',
          text: 'Clear them',
          onclick: () => {
            if (this.game?.erase(...hint.cells)) this.afterMove();
          },
        }),
      );
    }
    actions.append(
      h('button', {
        class: 'button ghost',
        text: 'Dismiss',
        onclick: () => {
          this.hint = null;
          this.renderAll();
        },
      }),
    );
    children.push(actions);
    this.hintCard.replaceChildren(...children);
  }
}
