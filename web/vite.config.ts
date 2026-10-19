import { defineConfig } from 'vitest/config';

export default defineConfig({
  build: { target: 'es2022' },
  // The engine worker imports the WebAssembly glue as an ES module.
  worker: { format: 'es' },
  test: { environment: 'node', include: ['tests/**/*.test.ts'] },
});
