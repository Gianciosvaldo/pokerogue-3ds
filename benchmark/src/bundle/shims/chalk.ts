// chalk stand-in: styles are no-ops. A call returns a value that works both as a string and as a
// further style function, since chalk.hex(color)(text) and chalk.red(text) share the same shape.
function styled(s: string): any {
  const f: any = (...a: any[]) => styled(a.join(" "));
  f.toString = () => s;
  f[Symbol.toPrimitive] = () => s;
  return new Proxy(f, { get: (t, p) => (p in t ? t[p] : identity) });
}
const identity: any = new Proxy((...args: any[]) => styled(args.join(" ")), {
  get: (_t, p) => (p === "level" ? 0 : identity),
  set: () => true,
});
export default identity;
export const foregroundColorNames: string[] = [];
export type ChalkInstance = any;
export type ForegroundColorName = string;
