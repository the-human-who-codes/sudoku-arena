let element: HTMLDivElement | null = null;
let timer = 0;

/** A short status message at the bottom of the screen, also announced to screen readers. */
export function toast(message: string) {
  if (!element) {
    element = document.createElement('div');
    element.className = 'toast';
    element.setAttribute('role', 'status');
    element.setAttribute('aria-live', 'polite');
    document.body.append(element);
  }
  element.textContent = message;
  element.classList.add('show');
  clearTimeout(timer);
  timer = window.setTimeout(() => element?.classList.remove('show'), 2200);
}

export async function copyText(text: string, message: string) {
  try {
    await navigator.clipboard.writeText(text);
    toast(message);
  } catch {
    window.prompt('Copy this:', text);
  }
}
