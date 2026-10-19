import type { EngineMethod, EngineReply, EngineRequest } from './protocol';
import type {
  Algorithm,
  AnalyzeResponse,
  BenchmarkResponse,
  GenerateResponse,
  HintResponse,
  Level,
  SolveResponse,
} from './types';

export class CancelledError extends Error {
  constructor() {
    super('Cancelled');
  }
}

interface Pending {
  resolve: (value: unknown) => void;
  reject: (error: Error) => void;
}

/** Promise API over the engine worker. Requests run one at a time, in order. */
export class Engine {
  private worker!: Worker;
  private nextId = 1;
  private readonly pending = new Map<number, Pending>();
  /** Resolves with the engine version once the WebAssembly module is loaded. */
  readonly ready: Promise<string>;

  constructor() {
    this.spawn();
    this.ready = this.call<{ version: string }>('version', []).then((reply) => reply.version);
  }

  solve(puzzle: string, algorithm: Algorithm, traceLimit = 250_000) {
    return this.call<SolveResponse>('solve', [puzzle, algorithm, traceLimit]);
  }

  generate(level: Level, seed: string) {
    return this.call<GenerateResponse>('generate', [level, seed]);
  }

  analyze(puzzle: string) {
    return this.call<AnalyzeResponse>('analyze', [puzzle]);
  }

  hint(puzzle: string, current: string) {
    return this.call<HintResponse>('hint', [puzzle, current]);
  }

  benchmark(puzzle: string) {
    return this.call<BenchmarkResponse>('benchmark', [puzzle]);
  }

  /** Abandons every request in flight by restarting the worker. */
  cancel() {
    this.worker.terminate();
    for (const { reject } of this.pending.values()) reject(new CancelledError());
    this.pending.clear();
    this.spawn();
  }

  get busy() {
    return this.pending.size > 0;
  }

  private spawn() {
    this.worker = new Worker(new URL('./worker.ts', import.meta.url), { type: 'module' });
    this.worker.onmessage = ({ data }: MessageEvent<EngineReply>) => {
      const pending = this.pending.get(data.id);
      if (!pending) return;
      this.pending.delete(data.id);
      if (data.ok) pending.resolve(data.result);
      else pending.reject(new Error(data.error));
    };
    this.worker.onerror = (event) => {
      const error = new Error(event.message || 'The engine failed to load.');
      for (const { reject } of this.pending.values()) reject(error);
      this.pending.clear();
    };
  }

  private call<T>(method: EngineMethod, args: (string | number)[]): Promise<T> {
    const id = this.nextId++;
    return new Promise<T>((resolve, reject) => {
      this.pending.set(id, { resolve: resolve as (value: unknown) => void, reject });
      this.worker.postMessage({ id, method, args } satisfies EngineRequest);
    });
  }
}

export const engine = new Engine();
