// Minimal stand-in for the parts of vitest the upstream test harness uses at runtime.
type AnyFn = (...args: any[]) => any;

function makeMock(impl?: AnyFn) {
  let current: AnyFn | undefined = impl;
  const once: AnyFn[] = [];
  const mock: any = function (this: any, ...args: any[]) {
    mock.mock.calls.push(args);
    const f = once.length > 0 ? once.shift() : current;
    return f ? f.apply(this, args) : undefined;
  };
  mock.mock = { calls: [] as any[], results: [] as any[] };
  mock.mockImplementation = (f: AnyFn) => ((current = f), mock);
  mock.mockImplementationOnce = (f: AnyFn) => (once.push(f), mock);
  mock.mockReturnValue = (v: any) => ((current = () => v), mock);
  mock.mockReturnValueOnce = (v: any) => (once.push(() => v), mock);
  mock.mockResolvedValue = (v: any) => ((current = () => Promise.resolve(v)), mock);
  mock.mockClear = () => ((mock.mock.calls = []), mock);
  mock.mockReset = () => mock;
  mock.mockRestore = () => mock;
  mock.mockName = () => mock;
  return mock;
}

function spyOn(obj: any, key: PropertyKey, accessType?: "get" | "set") {
  if (accessType === "get") {
    let desc: PropertyDescriptor | undefined;
    for (let o = obj; o && !desc; o = Object.getPrototypeOf(o)) {
      desc = Object.getOwnPropertyDescriptor(o, key);
    }
    const orig = desc?.get ? desc.get : () => desc?.value;
    const m = makeMock(function (this: any) {
      return orig.call(this);
    });
    try {
      Object.defineProperty(obj, key, { get: m, configurable: true, enumerable: true });
    } catch {}
    return m;
  }
  const orig = obj[key];
  const m = makeMock(
    typeof orig === "function"
      ? function (this: any, ...a: any[]) {
          return orig.apply(this, a);
        }
      : undefined,
  );
  try {
    obj[key] = m;
  } catch {}
  return m;
}

export const vi = {
  fn: (impl?: AnyFn) => makeMock(impl),
  spyOn,
  mock: () => {},
  unmock: () => {},
  restoreAllMocks: () => {},
  clearAllMocks: () => {},
  resetAllMocks: () => {},
  useFakeTimers: () => vi,
  useRealTimers: () => vi,
  advanceTimersByTime: () => vi,
  waitUntil: (cond: () => any, opts?: any) =>
    new Promise((resolve, reject) => {
      const timeout = typeof opts === "number" ? opts : (opts?.timeout ?? 20000);
      // Uses the host's virtual clock so a slow CPU never trips the timeout.
      const now = () => (globalThis as any).__vnow();
      const start = now();
      const check = (r: any) => {
        if (r) {
          return resolve(r);
        }
        if (now() - start > timeout) {
          return reject(new Error("waitUntil timed out"));
        }
        setTimeout(tick, 1);
      };
      const tick = () => {
        let r: any;
        try {
          r = cond();
        } catch (e) {
          return reject(e);
        }
        if (r && typeof r.then === "function") {
          r.then(check, reject);
        } else {
          check(r);
        }
      };
      tick();
    }),
};

// expect(...) never throws here: this is a benchmark, not a test run.
const noopMatcher: any = new Proxy(function () {}, {
  get: (_t, p) => (p === "then" ? undefined : noopMatcher),
  apply: () => noopMatcher,
});
export const expect: any = new Proxy(() => noopMatcher, {
  get: (_t, p) => (p === "fail" ? (msg: string) => { throw new Error(msg); } : noopMatcher),
});
export const describe = () => {};
export const it = () => {};
export const test = () => {};
export const beforeAll = () => {};
export const beforeEach = () => {};
export const afterAll = () => {};
export const afterEach = () => {};
export type MockInstance = any;
