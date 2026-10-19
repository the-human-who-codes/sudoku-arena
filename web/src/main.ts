import './styles.css';

import { engine } from './engine/client';
import { ICONS, h, icon } from './ui/dom';
import { DuelView } from './views/duel';
import { PlayView } from './views/play';
import { SolverView } from './views/solver';

interface View {
  readonly element: HTMLElement;
  show?(params: URLSearchParams): void;
  hide?(): void;
  onKeyDown?(event: KeyboardEvent): void;
}

const ROUTES = {
  solver: { label: 'Solver', create: () => new SolverView() },
  play: { label: 'Play', create: () => new PlayView() },
  duel: { label: 'Duel', create: () => new DuelView() },
} satisfies Record<string, { label: string; create: () => View }>;

type Route = keyof typeof ROUTES;

const views = new Map<Route, View>();
let active: Route | null = null;

const REPO_URL = 'https://github.com/the-human-who-codes/sudoku-arena';

// ---- Theme ------------------------------------------------------------------

const themeButton = h('button', { class: 'button ghost icon-only', 'aria-label': 'Toggle colour theme' });

function setTheme(theme: 'light' | 'dark', save = false) {
  document.documentElement.dataset.theme = theme;
  document.querySelector('meta[name="theme-color"]')?.setAttribute('content', theme === 'dark' ? '#0d0d0d' : '#f9f9f7');
  themeButton.replaceChildren(icon(theme === 'dark' ? ICONS.sun : ICONS.moon));
  if (save) localStorage.setItem('sudoku-arena:theme', theme);
}

themeButton.addEventListener('click', () =>
  setTheme(document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark', true),
);
setTheme(document.documentElement.dataset.theme === 'light' ? 'light' : 'dark');

// ---- Shell ------------------------------------------------------------------

const status = h('span', { class: 'engine-status', text: 'Loading engine…' });
engine.ready.then(
  (version) => {
    status.textContent = `C++ engine v${version} · WebAssembly`;
    status.dataset.state = 'ready';
  },
  () => {
    status.textContent = 'Engine failed to load';
    status.dataset.state = 'error';
  },
);

const navLinks = new Map<Route, HTMLAnchorElement>();
const nav = h('nav', { class: 'nav', 'aria-label': 'Main' });
for (const [route, { label }] of Object.entries(ROUTES) as [Route, (typeof ROUTES)[Route]][]) {
  const link = h('a', { href: `#/${route}` }, label, route === 'duel' ? h('span', { class: 'soon', text: 'soon' }) : null);
  navLinks.set(route, link);
  nav.append(link);
}

const main = h('main', { id: 'main' });
document.querySelector('#app')!.replaceChildren(
  h(
    'header',
    { class: 'topbar' },
    h(
      'a',
      { class: 'brand', href: '#/solver', 'aria-label': 'Sudoku Arena home' },
      h('img', { class: 'brand-mark', src: '/favicon.svg', alt: '' }),
      h('span', { text: 'Sudoku Arena' }),
    ),
    nav,
    h(
      'div',
      { class: 'topbar-end' },
      status,
      h('a', { class: 'button ghost icon-only', href: REPO_URL, 'aria-label': 'Source on GitHub', title: 'Source on GitHub' }, icon(ICONS.github)),
      themeButton,
    ),
  ),
  main,
  h(
    'footer',
    {},
    'Built by Muano Masiagwala. Engine in C++20 (logic solver, constraint propagation, Dancing Links, generator), compiled to WebAssembly with Emscripten; hosted on Firebase. ',
    h('a', { href: REPO_URL, text: 'Source' }),
  ),
);

// ---- Routing ------------------------------------------------------------------

function route() {
  const [path, query = ''] = window.location.hash.replace(/^#\/?/, '').split('?');
  const next: Route = path in ROUTES ? (path as Route) : 'solver';
  if (active !== next) {
    if (active) views.get(active)?.hide?.();
    let view = views.get(next);
    if (!view) {
      view = ROUTES[next].create();
      views.set(next, view);
    }
    main.replaceChildren(view.element);
    active = next;
    for (const [r, link] of navLinks) {
      if (r === next) link.setAttribute('aria-current', 'page');
      else link.removeAttribute('aria-current');
    }
    document.title = `${ROUTES[next].label} · Sudoku Arena`;
    window.scrollTo({ top: 0 });
  }
  views.get(next)?.show?.(new URLSearchParams(query));
}

window.addEventListener('hashchange', route);
route();

document.addEventListener('keydown', (event) => {
  const target = event.target as HTMLElement;
  if (target.closest('input, select, textarea, dialog')) return;
  if (target.closest('button') && (event.key === ' ' || event.key === 'Enter')) return;
  if (active) views.get(active)?.onKeyDown?.(event);
});
