// Runs the C++ engine (compiled to WebAssembly) off the main thread, so long
// solves never freeze the page.

import createSudokuEngine from './generated/sudoku.mjs';
import wasmUrl from './generated/sudoku.wasm?url';
import type { EngineReply, EngineRequest } from './protocol';

const engine = createSudokuEngine({ locateFile: () => wasmUrl });

self.onmessage = async ({ data }: MessageEvent<EngineRequest>) => {
  try {
    const module = await engine;
    const call = module[data.method] as (...args: unknown[]) => string;
    const result = JSON.parse(call(...data.args));
    if (!result.ok) throw new Error(result.error);

    // Search traces can hold hundreds of thousands of events: hand them over as a
    // transferable typed array instead of copying a giant JS array.
    const transfer: Transferable[] = [];
    if (result.trace?.kind === 'search') {
      result.trace.events = Uint32Array.from(result.trace.events);
      transfer.push(result.trace.events.buffer);
    }
    self.postMessage({ id: data.id, ok: true, result } satisfies EngineReply, { transfer });
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    self.postMessage({ id: data.id, ok: false, error: message } satisfies EngineReply);
  }
};
