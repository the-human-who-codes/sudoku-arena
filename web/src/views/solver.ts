import { CancelledError, engine } from '../engine/client';
import {
  ALGORITHMS,
  LEVELS,
  algorithmInfo,
  type Algorithm,
  type AnalyzeResponse,
  type Level,
  type LogicStep,
  type SolveResponse,
} from '../engine/types';
import { TracePlayer, traceOf, type StepView } from '../player/trace';
import { bit, cellName, conflicts, formatPuzzle, parsePuzzle, type Values } from '../sudoku/grid';
import { SAMPLES } from '../sudoku/samples';
import { BoardView, type BoardHighlight } from '../ui/board';
import { ProfileChart, raceChart } from '../ui/charts';
import { ICONS, h, icon } from '../ui/dom';
import { capitalize, compact, formatMicros } from '../ui/format';
import { copyText, toast } from '../ui/toast';

const SPEEDS = [1, 2, 4, 8, 15, 30, 60, 120, 250, 500, 1000, 2500, 5000, 10_000, 25_000, 50_000, 100_000];
/** Above this many steps per second, per-cell animation is switched off. */
const ANIMATION_LIMIT = 30;
/** A search replay's default speed makes it last about this long. */
const REPLAY_SECONDS = 12;

const SEARCH_LABELS = { guess: 'Guess', forced: 'Forced', undo: 'Undo' } as const;

export class SolverView {
  readonly element: HTMLElement;
  private puzzle: Values;
  private algorithm: Algorithm = 'logic';
  private analysis: AnalyzeResponse | null = null;
  private response: SolveResponse | null = null;
  private player: TracePlayer | null = null;
  private showingSolution = false;
  private playing = false;
  private speedIndex = 1;
  private budget = 0;
  private lastFrame = 0;
  private analyzeTimer = 0;
  private analysisToken = 0;
  private stepItems: HTMLLIElement[] = [];
  private markedStep = -1;

  private readonly board: BoardView;
  private readonly chart = new ProfileChart();
  private readonly caption = h('div', { class: 'caption', 'aria-live': 'polite' });
  private readonly playButton = h('button', { class: 'button primary play', 'aria-label': 'Play' });
  private readonly transportButtons: HTMLButtonElement[] = [];
  private readonly speedInput = h('input', { type: 'range', min: 0, max: SPEEDS.length - 1, step: 1, 'aria-label': 'Playback speed' });
  private readonly speedOutput = h('output');
  private readonly scrubInput = h('input', { type: 'range', min: 0, max: 0, step: 1, value: 0, 'aria-label': 'Step' });
  private readonly scrubOutput = h('output');
  private readonly legend = h('div', { class: 'legend', hidden: true });
  private readonly levelSelect = h('select', { class: 'field', 'aria-label': 'Difficulty' });
  private readonly sampleSelect = h('select', { class: 'field grow', 'aria-label': 'Famous puzzles' });
  private readonly importInput = h('input', {
    class: 'field grow',
    placeholder: 'Paste 81 characters: digits, and 0 or . for blanks',
    'aria-label': 'Puzzle text',
    spellcheck: 'false',
  });
  private readonly analysisLine = h('p', { class: 'analysis' });
  private readonly algorithmButtons = new Map<Algorithm, HTMLButtonElement>();
  private readonly algorithmText = h('p', { class: 'algo-description' });
  private readonly solveButton = h('button', { class: 'button primary wide' });
  private readonly cancelButton = h('button', { class: 'button ghost wide', hidden: true, text: 'Cancel' });
  private readonly resultsCard = h('section', { class: 'card', hidden: true, 'aria-label': 'Result' });
  private readonly stepsCard = h('section', { class: 'card', hidden: true });
  private readonly stepsList = h('ol', { class: 'steps' });
  private readonly raceBody = h('div');
  private readonly raceButton = h('button', { class: 'button', text: 'Race all four engines' });

  constructor() {
    this.puzzle = parsePuzzle(SAMPLES[0].puzzle)!;
    this.board = new BoardView({
      label: 'Puzzle board. Select a cell and type a digit to edit the puzzle.',
      onSelect: (cell) => this.board.select(cell, { peers: false, sameDigit: false }),
      onKey: (event, cell) => this.onBoardKey(event, cell),
    });
    this.chart.onSeek = (position) => {
      this.pause();
      this.seek(position);
    };

    this.element = h(
      'section',
      { class: 'view solver' },
      h(
        'header',
        { class: 'view-head' },
        h('h1', { text: 'Watch a C++ engine solve Sudoku' }),
        h('p', {
          class: 'lede',
          text: 'Four algorithms, written in C++ and compiled to WebAssembly, running right here in your browser. Pick one and replay every move it makes.',
        }),
      ),
      h('div', { class: 'split' }, this.buildStage(), this.buildPanel()),
    );

    this.setSpeed(1);
    this.setAlgorithm('logic');
    this.loadPuzzle(this.puzzle);
  }

  /** Applies `#/solver?p=<puzzle>&algo=<id>` parameters. */
  show(params: URLSearchParams) {
    const puzzle = parsePuzzle(params.get('p') ?? '');
    const algo = params.get('algo') as Algorithm | null;
    if (algo && ALGORITHMS.some((a) => a.id === algo)) this.setAlgorithm(algo);
    if (puzzle) this.loadPuzzle(puzzle);
  }

  hide() {
    this.pause();
  }

  onKeyDown(event: KeyboardEvent) {
    if (event.defaultPrevented || event.metaKey || event.ctrlKey || event.altKey) return;
    if (event.key === ' ') {
      event.preventDefault();
      this.toggle();
    } else if (event.key === '.' || event.key === ']') {
      this.pause();
      this.seek((this.player?.position ?? 0) + 1);
    } else if (event.key === ',' || event.key === '[') {
      this.pause();
      this.seek((this.player?.position ?? 0) - 1);
    }
  }

  // ---- Layout ---------------------------------------------------------------

  private buildStage() {
    const button = (paths: string, label: string, action: () => void) => {
      const element = h('button', { class: 'button ghost icon-only', 'aria-label': label, title: label, onclick: action }, icon(paths));
      this.transportButtons.push(element);
      return element;
    };
    this.playButton.addEventListener('click', () => this.toggle());
    this.transportButtons.push(this.playButton);
    this.speedInput.addEventListener('input', () => this.setSpeed(Number(this.speedInput.value)));
    this.scrubInput.addEventListener('input', () => {
      this.pause();
      this.seek(Number(this.scrubInput.value));
    });
    this.legend.append(
      ...(['guess', 'forced', 'undo'] as const).map((kind) =>
        h('span', {}, h('i', { style: `background: var(--${kind === 'undo' ? 'critical' : kind})` }), SEARCH_LABELS[kind]),
      ),
    );

    return h(
      'div',
      { class: 'stage' },
      this.board.element,
      this.caption,
      h(
        'div',
        { class: 'transport' },
        h(
          'div',
          { class: 'row' },
          button(ICONS.skipStart, 'Back to start', () => {
            this.pause();
            this.seek(0);
          }),
          button(ICONS.stepBack, 'Previous step', () => {
            this.pause();
            this.seek((this.player?.position ?? 0) - 1);
          }),
          this.playButton,
          button(ICONS.stepForward, 'Next step', () => {
            this.pause();
            this.seek((this.player?.position ?? 0) + 1);
          }),
          button(ICONS.skipEnd, 'Jump to the end', () => {
            this.pause();
            this.seek(this.player?.trace.length ?? 0);
          }),
          h('label', { class: 'speed' }, 'Speed', this.speedInput, this.speedOutput),
        ),
        h('div', { class: 'scrub' }, this.scrubInput, this.scrubOutput),
        this.legend,
      ),
    );
  }

  private buildPanel() {
    for (const level of LEVELS) this.levelSelect.append(h('option', { value: level, text: capitalize(level) }));
    this.levelSelect.value = 'hard';
    this.sampleSelect.append(h('option', { value: '', text: 'Famous puzzles…' }));
    SAMPLES.forEach((sample, i) => this.sampleSelect.append(h('option', { value: i, text: sample.name })));
    this.sampleSelect.addEventListener('change', () => {
      const sample = SAMPLES[Number(this.sampleSelect.value)];
      if (sample) {
        this.loadPuzzle(parsePuzzle(sample.puzzle)!);
        toast(sample.blurb);
      }
    });
    const importPuzzle = () => {
      const puzzle = parsePuzzle(this.importInput.value);
      if (puzzle) this.loadPuzzle(puzzle);
      else toast('That isn’t 81 cells. Use digits for givens and 0 or . for blanks.');
    };
    this.importInput.addEventListener('keydown', (event) => {
      if (event.key === 'Enter') importPuzzle();
    });

    const puzzleCard = h(
      'section',
      { class: 'card' },
      h('h2', { text: 'Puzzle' }),
      h(
        'div',
        { class: 'row' },
        this.levelSelect,
        h('button', { class: 'button', onclick: () => this.generate() }, icon(ICONS.sparkles), 'Generate'),
        this.sampleSelect,
      ),
      h('div', { class: 'row' }, this.importInput, h('button', { class: 'button', text: 'Load', onclick: importPuzzle })),
      h(
        'div',
        { class: 'row' },
        h('button', { class: 'button ghost', text: 'Clear board', onclick: () => this.loadPuzzle(new Array(81).fill(0)) }),
        h('button', {
          class: 'button ghost',
          onclick: () => copyText(formatPuzzle(this.puzzle), 'Puzzle copied'),
        }, icon(ICONS.copy), 'Copy'),
        h('button', {
          class: 'button ghost',
          onclick: () => copyText(this.shareLink(), 'Link copied'),
        }, icon(ICONS.link), 'Share link'),
      ),
      this.analysisLine,
    );

    const segmented = h('div', { class: 'segmented', role: 'radiogroup', 'aria-label': 'Algorithm' });
    for (const info of ALGORITHMS) {
      const button = h('button', {
        role: 'radio',
        'aria-checked': 'false',
        text: info.short,
        title: info.name,
        onclick: () => this.setAlgorithm(info.id),
      });
      this.algorithmButtons.set(info.id, button);
      segmented.append(button);
    }
    this.solveButton.addEventListener('click', () => this.solve());
    this.cancelButton.addEventListener('click', () => engine.cancel());

    const algorithmCard = h(
      'section',
      { class: 'card' },
      h('h2', { text: 'Algorithm' }),
      segmented,
      this.algorithmText,
      this.solveButton,
      this.cancelButton,
    );

    this.stepsCard.append(h('h2', { text: 'Deductions' }), this.stepsList);
    this.raceButton.addEventListener('click', () => this.race());
    const raceCard = h(
      'section',
      { class: 'card' },
      h('h2', { text: 'Engine race' }),
      h('p', { class: 'muted small', text: 'The same puzzle through every algorithm, timed inside WebAssembly. Fast solvers are averaged over many runs.' }),
      h('div', { class: 'row' }, this.raceButton),
      this.raceBody,
    );

    return h('aside', { class: 'panel' }, puzzleCard, algorithmCard, this.resultsCard, this.stepsCard, raceCard);
  }

  // ---- Puzzle -----------------------------------------------------------------

  private loadPuzzle(puzzle: Values) {
    this.pause();
    this.puzzle = [...puzzle];
    this.clearSolve();
    const text = formatPuzzle(this.puzzle);
    const sample = SAMPLES.findIndex((s) => s.puzzle === text);
    this.sampleSelect.value = sample >= 0 ? String(sample) : '';
    this.importInput.value = '';
    this.raceBody.replaceChildren();
    this.renderBoard();
    this.scheduleAnalysis(0);
  }

  private async generate() {
    const level = this.levelSelect.value as Level;
    try {
      const seed = Math.floor(Math.random() * 2 ** 32).toString(36);
      const generated = await engine.generate(level, seed);
      this.loadPuzzle(parsePuzzle(generated.puzzle)!);
      toast(`${capitalize(generated.rating.level)} puzzle, ${generated.clues} givens, built in ${formatMicros(generated.micros)}`);
    } catch (error) {
      if (!(error instanceof CancelledError)) toast(String(error));
    }
  }

  private onBoardKey(event: KeyboardEvent, cell: number) {
    let digit: number | null = null;
    if (/^[1-9]$/.test(event.key)) digit = Number(event.key);
    else if (['Backspace', 'Delete', '0', '.'].includes(event.key)) digit = 0;
    if (digit === null || event.metaKey || event.ctrlKey) return;
    event.preventDefault();
    if (this.puzzle[cell] === digit) return;
    this.puzzle[cell] = digit;
    this.pause();
    this.clearSolve();
    this.sampleSelect.value = '';
    this.renderBoard();
    this.scheduleAnalysis(300);
  }

  private scheduleAnalysis(delay: number) {
    clearTimeout(this.analyzeTimer);
    this.analyzeTimer = window.setTimeout(() => this.analyze(), delay);
  }

  private async analyze() {
    const token = ++this.analysisToken;
    try {
      const analysis = await engine.analyze(formatPuzzle(this.puzzle));
      if (token !== this.analysisToken) return;
      this.analysis = analysis;
      this.renderAnalysis();
    } catch (error) {
      if (!(error instanceof CancelledError)) this.analysisLine.textContent = String(error);
    }
  }

  private renderAnalysis() {
    const a = this.analysis;
    if (!a) return;
    const items: HTMLElement[] = [h('span', { text: `${a.clues} givens` })];
    if (!a.valid) items.push(h('span', { class: 'bad', text: `${a.conflicts.length} givens clash` }));
    else if (a.solutions === 0) items.push(h('span', { class: 'bad', text: 'No solution' }));
    else if (a.solutions > 1) items.push(h('span', { class: 'bad', text: 'More than one solution' }));
    else items.push(h('span', { text: 'Unique solution' }));
    if (a.rating) items.push(h('span', { text: `${capitalize(a.rating.level)} · needs ${a.rating.hardestName}` }));
    this.analysisLine.replaceChildren(...items);
  }

  private shareLink() {
    const url = new URL(window.location.href);
    url.hash = `/solver?p=${formatPuzzle(this.puzzle)}&algo=${this.algorithm}`;
    return url.toString();
  }

  // ---- Solving ------------------------------------------------------------------

  private setAlgorithm(algorithm: Algorithm) {
    this.algorithm = algorithm;
    for (const [id, button] of this.algorithmButtons) button.setAttribute('aria-checked', String(id === algorithm));
    const info = algorithmInfo(algorithm);
    this.algorithmText.textContent = info.description;
    this.solveButton.replaceChildren(icon(ICONS.play), `Solve with ${info.name.toLowerCase()}`);
  }

  private async solve() {
    this.pause();
    const info = algorithmInfo(this.algorithm);
    this.solveButton.disabled = true;
    this.solveButton.replaceChildren(`Solving with ${info.name.toLowerCase()}…`);
    const showCancel = window.setTimeout(() => (this.cancelButton.hidden = false), 400);
    try {
      this.loadResponse(await engine.solve(formatPuzzle(this.puzzle), this.algorithm));
    } catch (error) {
      toast(error instanceof CancelledError ? 'Solve cancelled' : String(error));
    } finally {
      clearTimeout(showCancel);
      this.cancelButton.hidden = true;
      this.solveButton.disabled = false;
      this.setAlgorithm(this.algorithm);
    }
  }

  private loadResponse(response: SolveResponse) {
    this.response = response;
    const trace = traceOf(response);
    this.player = new TracePlayer(trace);
    this.showingSolution = false;
    this.markedStep = -1;
    this.chart.setData(trace.length ? trace.filledCounts() : null);
    this.scrubInput.max = String(trace.length);
    this.legend.hidden = response.trace.kind !== 'search';
    if (response.trace.kind === 'logic') {
      this.setSpeed(1);
    } else {
      const wanted = trace.length / REPLAY_SECONDS;
      const index = SPEEDS.findIndex((speed) => speed >= wanted);
      this.setSpeed(index < 0 ? SPEEDS.length - 1 : Math.max(index, 3));
    }
    this.renderResults();
    this.renderSteps();
    this.update(null);
    if (trace.length) this.play();
  }

  private clearSolve() {
    this.response = null;
    this.player = null;
    this.showingSolution = false;
    this.resultsCard.hidden = true;
    this.stepsCard.hidden = true;
    this.stepItems = [];
    this.markedStep = -1;
    this.stepsList.replaceChildren();
    this.legend.hidden = true;
    this.chart.setData(null);
    this.scrubInput.max = '0';
    this.board.highlight(null);
    this.update(null);
  }

  private async race() {
    this.raceButton.disabled = true;
    this.raceButton.textContent = 'Racing…';
    try {
      const { results } = await engine.benchmark(formatPuzzle(this.puzzle));
      this.raceBody.replaceChildren(raceChart(results));
    } catch (error) {
      if (!(error instanceof CancelledError)) toast(String(error));
    } finally {
      this.raceButton.disabled = false;
      this.raceButton.textContent = 'Race all four engines';
    }
  }

  // ---- Playback -----------------------------------------------------------------

  private toggle() {
    if (this.playing) this.pause();
    else this.play();
  }

  private play() {
    if (!this.player || this.player.trace.length === 0) return;
    if (this.player.atEnd) this.seek(0);
    this.playing = true;
    this.budget = 1; // take the first step right away
    this.lastFrame = performance.now();
    requestAnimationFrame((now) => this.tick(now));
    this.updateTransport();
  }

  private pause() {
    if (!this.playing) return;
    this.playing = false;
    this.updateTransport();
  }

  private tick(now: number) {
    if (!this.playing || !this.player) return;
    this.budget += (Math.min(now - this.lastFrame, 250) / 1000) * SPEEDS[this.speedIndex];
    this.lastFrame = now;
    const steps = Math.floor(this.budget);
    if (steps > 0) {
      this.budget -= steps;
      this.seek(this.player.position + steps);
    }
    if (this.player.atEnd) this.pause();
    else requestAnimationFrame((next) => this.tick(next));
  }

  private setSpeed(index: number) {
    this.speedIndex = index;
    this.speedInput.value = String(index);
    const speed = SPEEDS[index];
    this.speedOutput.textContent = `${compact(speed)} step${speed === 1 ? '' : 's'}/s`;
    this.board.animate = speed <= ANIMATION_LIMIT;
  }

  private seek(position: number) {
    if (!this.player) return;
    const changed = this.player.seek(position);
    this.update(changed);
  }

  // ---- Rendering ------------------------------------------------------------------

  private update(changed: Set<number> | null) {
    const player = this.player;
    const response = this.response;
    // A truncated replay ends before the solve did: finish on the real solution.
    const finish =
      !!player && !!response && player.atEnd && response.trace.kind === 'search' && response.trace.truncated && !!response.solution;
    if (finish !== this.showingSolution) {
      this.showingSolution = finish;
      changed = null;
    }
    this.renderBoard(changed ?? undefined);
    this.board.highlight(this.showingSolution ? null : this.highlightFor(player?.current ?? null));
    this.renderCaption();
    this.updateTransport();
    this.chart.setPosition(player?.position ?? 0);
    this.markStep();
  }

  private renderBoard(cells: Iterable<number> = Array.from({ length: 81 }, (_, i) => i)) {
    const player = this.player;
    if (!player) {
      const clashes = conflicts(this.puzzle);
      for (const cell of cells) this.board.render(cell, this.puzzle[cell], 0, clashes.has(cell) ? 'wrong' : 'given');
      return;
    }
    const solution = this.showingSolution && this.response?.solution ? parsePuzzle(this.response.solution) : null;
    const { values, candidates } = player.state;
    const marks = player.trace.showsCandidates;
    for (const cell of cells) {
      const value = solution ? solution[cell] : values[cell];
      this.board.render(cell, value, marks ? candidates[cell] : 0, this.puzzle[cell] ? 'given' : 'solver');
    }
  }

  private highlightFor(view: StepView | null): BoardHighlight | null {
    if (!view) return null;
    if (view.kind !== 'logic') return { event: view };
    const step = view.step;
    const candidates = this.player!.state.candidates;
    const focus: [number, number][] = [];
    for (const cell of step.cells) {
      for (const digit of step.digits) if (candidates[cell] & bit(digit)) focus.push([cell, digit]);
    }
    return {
      units: step.units,
      cells: step.cells,
      focus,
      eliminated: step.eliminate,
      placed: step.place.map(([cell]) => cell),
    };
  }

  private renderCaption() {
    const player = this.player;
    const response = this.response;
    const set = (badge: HTMLElement | null, text: string) =>
      this.caption.replaceChildren(...(badge ? [badge] : []), h('span', { class: 'text', text }));

    if (!player || !response) {
      set(null, `Press solve to watch ${algorithmInfo(this.algorithm).name.toLowerCase()} at work. Click any cell and type to edit the puzzle.`);
      return;
    }
    if (player.trace.length === 0) {
      const reasons: Record<string, string> = {
        invalid: 'This puzzle repeats a digit in a row, column or box, so there is nothing to solve.',
        unsolvable: 'The engine proved this puzzle has no solution.',
        solved: 'Already solved.',
      };
      set(null, reasons[response.status] ?? `Stopped: ${response.status}.`);
      return;
    }
    if (this.showingSolution && response.trace.kind === 'search') {
      set(
        h('span', { class: 'badge' }, 'Solved'),
        `The replay keeps the first ${compact(player.trace.length)} of ${compact(response.stats.placements)} placements. The engine ran the whole search; this is the solution it reached.`,
      );
      return;
    }
    const view = player.current;
    if (!view) {
      set(null, `${player.trace.length.toLocaleString()} steps ready. Press play, or step through them one by one.`);
      return;
    }
    if (view.kind === 'logic') {
      set(h('span', { class: 'badge' }, view.step.name), stripName(view.step));
    } else {
      const where = cellName(view.cell);
      const text =
        view.kind === 'guess'
          ? `Try ${view.digit} in ${where}.`
          : view.kind === 'forced'
            ? `${view.digit} is the only option left for ${where}.`
            : `Dead end: take ${view.digit} back out of ${where}.`;
      set(h('span', { class: 'badge', 'data-kind': view.kind }, h('i', { class: 'key' }), SEARCH_LABELS[view.kind]), text);
    }
  }

  private updateTransport() {
    const length = this.player?.trace.length ?? 0;
    const position = this.player?.position ?? 0;
    for (const button of this.transportButtons) button.disabled = length === 0;
    this.playButton.replaceChildren(icon(this.playing ? ICONS.pause : ICONS.play));
    this.playButton.setAttribute('aria-label', this.playing ? 'Pause' : 'Play');
    this.scrubInput.disabled = length === 0;
    this.scrubInput.value = String(position);
    this.scrubOutput.textContent = length ? `Step ${position.toLocaleString()} / ${length.toLocaleString()}` : 'No replay yet';
  }

  private renderResults() {
    const response = this.response;
    if (!response) return;
    const tile = (label: string, value: string) =>
      h('div', { class: 'stat' }, h('div', { class: 'label', text: label }), h('div', { class: 'value', text: value }));
    const status = h('span', {
      class: 'status-chip',
      'data-status': response.status,
      text: { solved: 'Solved', unsolvable: 'No solution', invalid: 'Invalid', stuck: 'Stuck', limit: 'Gave up' }[response.status],
    });
    const work =
      response.algorithm === 'logic'
        ? tile('Steps', compact(response.stats.steps))
        : tile('Placements', compact(response.stats.placements));
    const children: (HTMLElement | null)[] = [
      h('h2', {}, algorithmInfo(response.algorithm).name, status),
      h(
        'div',
        { class: 'stats' },
        tile('Engine time', formatMicros(response.micros)),
        work,
        tile('Guesses', compact(response.stats.guesses)),
        tile('Backtracks', compact(response.stats.backtracks)),
      ),
    ];

    if (response.algorithm === 'logic') {
      if (response.rating) {
        children.push(h('p', { class: 'note', text: `Rated ${response.rating.level}: the hardest technique it needed was ${response.rating.hardestName}.` }));
      }
      const groups = h('div', { class: 'technique-groups' });
      for (const level of LEVELS) {
        const used = response.techniques.filter((t) => t.level === level && t.id !== 'backtrack');
        if (!used.length) continue;
        groups.append(
          h(
            'div',
            { class: 'technique-group' },
            h('span', { class: 'level', text: level }),
            ...used.map((t) => h('span', { class: 'chip' }, t.name, h('strong', { text: `×${t.count}` }))),
          ),
        );
      }
      children.push(groups);
    }
    if (this.player?.trace.length) {
      children.push(h('h3', { text: 'Cells filled over time' }), this.chart.element);
    }
    if (response.trace.kind === 'search' && response.trace.truncated) {
      children.push(
        h('p', {
          class: 'note',
          text: `The replay keeps the first ${compact(response.trace.events.length)} events; the full search made ${compact(response.stats.placements)} placements.`,
        }),
      );
    }
    if (response.status === 'limit') {
      children.push(h('p', { class: 'note', text: 'Naive backtracking stops after 100 million placements so the page stays responsive.' }));
    }
    this.resultsCard.replaceChildren(...children.filter((c): c is HTMLElement => c !== null));
    this.resultsCard.hidden = false;
  }

  private renderSteps() {
    const response = this.response;
    this.stepItems = [];
    this.stepsList.replaceChildren();
    if (!response || response.trace.kind !== 'logic' || response.trace.steps.length === 0) {
      this.stepsCard.hidden = true;
      return;
    }
    response.trace.steps.forEach((step, i) => {
      const item = h(
        'li',
        { class: 'future' },
        h(
          'button',
          {
            onclick: () => {
              this.pause();
              this.seek(i + 1);
              this.revealBoard();
            },
          },
          h('span', { class: 'index', text: i + 1 }),
          h('span', {}, h('span', { class: 'name', text: step.name }), ' ', stripName(step)),
        ),
      );
      this.stepItems.push(item);
    });
    this.stepsList.append(...this.stepItems);
    this.stepsCard.hidden = false;
  }

  /** On narrow screens the list sits below the board; bring the board back into view. */
  private revealBoard() {
    const box = this.board.element.getBoundingClientRect();
    if (box.bottom < 80 || box.top > window.innerHeight) {
      this.board.element.scrollIntoView({ behavior: 'smooth', block: 'start' });
    }
  }

  /** Moves the "current" marker in the deductions list, touching only what changed. */
  private markStep() {
    if (!this.stepItems.length) return;
    const current = (this.player?.position ?? 0) - 1;
    const from = Math.min(this.markedStep, current);
    const to = Math.max(this.markedStep, current);
    for (let i = Math.max(from, 0); i <= to && i < this.stepItems.length; i++) {
      this.stepItems[i].classList.toggle('current', i === current);
      this.stepItems[i].classList.toggle('future', i > current);
    }
    this.markedStep = current;
    const item = this.stepItems[current];
    if (item) {
      const list = this.stepsList;
      const top = item.offsetTop - list.offsetTop;
      if (top < list.scrollTop || top + item.offsetHeight > list.scrollTop + list.clientHeight) {
        list.scrollTop = top - list.clientHeight / 2 + item.offsetHeight / 2;
      }
    }
  }
}

/** Explanations start with the technique's name, which the badge already shows. */
function stripName(step: LogicStep) {
  const prefix = `${step.name}: `;
  return step.text.startsWith(prefix) ? step.text.slice(prefix.length) : step.text;
}
