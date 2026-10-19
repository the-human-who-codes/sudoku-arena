import { ICONS, h, icon } from '../ui/dom';
import { challengeLink, dailySeed } from './play';

const FEATURES = [
  {
    icon: ICONS.bolt,
    title: 'Same puzzle, guaranteed',
    text: 'Puzzles come from a seeded C++ generator with its own platform-independent random numbers, so a seed yields the identical puzzle in every browser, the CLI and on a server.',
  },
  {
    icon: ICONS.swords,
    title: 'Live head-to-head',
    text: 'Both players race on the same grid and watch each other’s progress as a count of filled cells, never the digits, synced through Firebase in real time.',
  },
  {
    icon: ICONS.trophy,
    title: 'Fair by construction',
    text: 'The server will check moves with this very engine: its WebAssembly build already runs under Node, so Cloud Functions can use it, and clients never need to hold the solution.',
  },
];

const ROADMAP: [string, boolean][] = [
  ['Seeded, deterministic puzzle generator (C++ → WebAssembly)', true],
  ['Shareable challenge links: same puzzle, beat my time', true],
  ['Accounts with Firebase Authentication', false],
  ['Rooms and matchmaking on Firebase Realtime Database', false],
  ['Server-side move validation in Cloud Functions', false],
  ['Ratings, ladders and rematches', false],
];

export class DuelView {
  readonly element: HTMLElement;

  constructor() {
    this.element = h(
      'section',
      { class: 'view duel' },
      h(
        'div',
        { class: 'duel-hero' },
        h('span', { class: 'eyebrow', text: 'Coming next' }),
        h('h1', { text: 'Sudoku Duel: two players, one puzzle' }),
        h('p', {
          text: 'Competitive Sudoku in real time. You and an opponent get the exact same puzzle at the same moment. The first to fill every cell correctly wins, and mistakes cost seconds.',
        }),
        h(
          'div',
          { class: 'row' },
          h('a', { class: 'button primary', href: '#/play' }, icon(ICONS.swords), 'Start a challenge'),
          h('a', {
            class: 'button',
            href: new URL(challengeLink('hard', dailySeed())).hash,
          }, icon(ICONS.calendar), 'Today’s shared puzzle'),
        ),
      ),
      h(
        'div',
        { class: 'feature-grid' },
        ...FEATURES.map((f) =>
          h('section', { class: 'card feature' }, icon(f.icon), h('h2', { text: f.title }), h('p', { text: f.text })),
        ),
      ),
      h(
        'section',
        { class: 'card', style: 'margin-top: 16px' },
        h('h2', { text: 'Roadmap' }),
        h('ul', { class: 'roadmap' }, ...ROADMAP.map(([text, done]) => h('li', { class: done ? 'done' : '', text }))),
        h('p', {
          class: 'muted small',
          style: 'margin-top: 14px',
          text: 'Until live duels land: finish a puzzle, copy the challenge link, and your friend plays the same grid against your time.',
        }),
      ),
    );
  }
}
