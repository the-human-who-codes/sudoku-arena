import type { GameData } from './game';

const GAME_KEY = 'sudoku-arena:game';
const SETTINGS_KEY = 'sudoku-arena:settings';

export interface Settings {
  showMistakes: boolean;
  highlightPeers: boolean;
  clearNotes: boolean;
}

export const DEFAULT_SETTINGS: Settings = { showMistakes: true, highlightPeers: true, clearNotes: true };

function read<T>(key: string): T | null {
  try {
    const raw = localStorage.getItem(key);
    return raw ? (JSON.parse(raw) as T) : null;
  } catch {
    return null;
  }
}

function write(key: string, value: unknown) {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch {
    // Private browsing or a full quota: progress just won't persist.
  }
}

export const loadGame = () => read<GameData>(GAME_KEY);
export const saveGame = (data: GameData) => write(GAME_KEY, data);
export const loadSettings = (): Settings => ({ ...DEFAULT_SETTINGS, ...read<Partial<Settings>>(SETTINGS_KEY) });
export const saveSettings = (settings: Settings) => write(SETTINGS_KEY, settings);
