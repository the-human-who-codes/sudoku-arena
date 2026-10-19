type Child = Node | string | number | null | undefined | false;
type Props = Record<string, unknown>;

/**
 * Creates an element. Props: `class`, `text`, `on<event>` listeners, and
 * attributes (true adds a bare attribute, false/null/undefined skips it).
 * Strings are always inserted as text, never parsed as HTML.
 */
export function h<K extends keyof HTMLElementTagNameMap>(
  tag: K,
  props: Props = {},
  ...children: Child[]
): HTMLElementTagNameMap[K] {
  const element = document.createElement(tag);
  for (const [key, value] of Object.entries(props)) {
    if (value === false || value === null || value === undefined) continue;
    if (key === 'class') element.className = String(value);
    else if (key === 'text') element.textContent = String(value);
    else if (key.startsWith('on') && typeof value === 'function') {
      element.addEventListener(key.slice(2), value as EventListener);
    } else element.setAttribute(key, value === true ? '' : String(value));
  }
  for (const child of children) {
    if (child === null || child === undefined || child === false) continue;
    element.append(child instanceof Node ? child : String(child));
  }
  return element;
}

const SVG_NS = 'http://www.w3.org/2000/svg';

export function svg<K extends keyof SVGElementTagNameMap>(
  tag: K,
  attrs: Record<string, string | number> = {},
  ...children: (SVGElement | null)[]
): SVGElementTagNameMap[K] {
  const element = document.createElementNS(SVG_NS, tag);
  for (const [key, value] of Object.entries(attrs)) element.setAttribute(key, String(value));
  for (const child of children) if (child) element.append(child);
  return element;
}

/** Inline icon from a static, trusted SVG path list (24×24 viewBox, stroked). */
export function icon(paths: string, label?: string): SVGSVGElement {
  const element = svg('svg', {
    viewBox: '0 0 24 24',
    width: 18,
    height: 18,
    fill: 'none',
    stroke: 'currentColor',
    'stroke-width': 2,
    'stroke-linecap': 'round',
    'stroke-linejoin': 'round',
    class: 'icon',
  });
  if (label) element.setAttribute('aria-label', label);
  else element.setAttribute('aria-hidden', 'true');
  for (const d of paths.split('|')) element.append(svg('path', { d }));
  return element;
}

export const ICONS = {
  play: 'M7 4.5v15l12-7.5z',
  pause: 'M8 5v14|M16 5v14',
  stepForward: 'M5 5l10 7-10 7z|M19 5v14',
  stepBack: 'M19 5L9 12l10 7z|M5 5v14',
  skipStart: 'M18 5l-8 7 8 7|M11 5l-8 7 8 7',
  skipEnd: 'M6 5l8 7-8 7|M13 5l8 7-8 7',
  undo: 'M9 14L4 9l5-5|M4 9h10.5a5.5 5.5 0 0 1 0 11H11',
  redo: 'M15 14l5-5-5-5|M20 9H9.5a5.5 5.5 0 0 0 0 11H13',
  erase: 'M20 20H8.5L3.6 15.1a2 2 0 0 1 0-2.8l9.5-9.5a2 2 0 0 1 2.8 0l4.5 4.5a2 2 0 0 1 0 2.8L12 18.5|M9 9l7 7',
  pencil: 'M12 20h9|M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z',
  bulb: 'M9 18h6|M10 22h4|M12 2a7 7 0 0 0-4 12.7c.6.5 1 1.2 1 2V17h6v-.3c0-.8.4-1.5 1-2A7 7 0 0 0 12 2z',
  sparkles: 'M12 3l1.9 5.1L19 10l-5.1 1.9L12 17l-1.9-5.1L5 10l5.1-1.9z|M19 15l.9 2.1L22 18l-2.1.9L19 21l-.9-2.1L16 18l2.1-.9z',
  copy: 'M9 9h11v11H9z|M5 15H4V4h11v1',
  link: 'M10 14a5 5 0 0 0 7.1 0l3-3a5 5 0 0 0-7.1-7.1l-1 1|M14 10a5 5 0 0 0-7.1 0l-3 3a5 5 0 0 0 7.1 7.1l1-1',
  sun: 'M12 17a5 5 0 1 0 0-10 5 5 0 0 0 0 10z|M12 1v2|M12 21v2|M4.2 4.2l1.4 1.4|M18.4 18.4l1.4 1.4|M1 12h2|M21 12h2|M4.2 19.8l1.4-1.4|M18.4 5.6l1.4-1.4',
  moon: 'M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z',
  github:
    'M9 19c-5 1.5-5-2.5-7-3m14 6v-3.9a3.4 3.4 0 0 0-.9-2.6c3.1-.4 6.4-1.5 6.4-7a5.4 5.4 0 0 0-1.5-3.7 5 5 0 0 0-.1-3.7s-1.2-.4-3.9 1.5a13.4 13.4 0 0 0-7 0C6.3 1.6 5.1 2 5.1 2a5 5 0 0 0-.1 3.7A5.4 5.4 0 0 0 3.5 9.4c0 5.4 3.3 6.6 6.4 7a3.4 3.4 0 0 0-.9 2.6V22',
  calendar: 'M4 5h16v16H4z|M4 10h16|M9 3v4|M15 3v4',
  trophy: 'M8 21h8|M12 17v4|M7 4h10v5a5 5 0 0 1-10 0z|M17 5h3a3 3 0 0 1-3 4|M7 5H4a3 3 0 0 0 3 4',
  swords: 'M14.5 17.5L3 6V3h3l11.5 11.5|M13 19l6-6|M16 16l4 4|M19 21l2-2|M9.5 6.5L14 2h3v3l-4.5 4.5|M5 14l-2 2 2 2|M7 16l-3 3',
  bolt: 'M13 2L4 14h7l-1 8 9-12h-7z',
  close: 'M18 6L6 18|M6 6l12 12',
} as const;
