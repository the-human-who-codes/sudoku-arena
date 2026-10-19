export type EngineMethod = 'solve' | 'generate' | 'analyze' | 'hint' | 'benchmark' | 'version';

export interface EngineRequest {
  id: number;
  method: EngineMethod;
  args: (string | number)[];
}

export type EngineReply =
  | { id: number; ok: true; result: unknown }
  | { id: number; ok: false; error: string };
