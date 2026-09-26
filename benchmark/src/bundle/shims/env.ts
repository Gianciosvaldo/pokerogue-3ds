// Browser/DOM stand-ins for running PokeRogue + Phaser (HEADLESS) inside a bare QuickJS host.
// The host (host.c) provides globalThis.__host = { print, now, gc, mem } and drives __runTimers().
const g: any = globalThis;
const host = g.__host;

// ---- deterministic Math.random so PC and 3DS replay exactly the same battles ----
let rngState = 0x9e3779b9;
Math.random = () => {
  rngState = (rngState + 0x6d2b79f5) | 0;
  let t = rngState;
  t = Math.imul(t ^ (t >>> 15), t | 1);
  t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
};

// ---- virtual-time timers: never sleep, just run the next due callback ----
let vnow = 0;
let nextId = 1;
type Timer = { id: number; at: number; fn: Function; args: any[]; every: number; seq: number };
const timers = new Map<number, Timer>();
let seq = 0;
function addTimer(fn: any, ms: any, args: any[], repeat: boolean) {
  const delay = Math.max(1, Number(ms) || 0);
  const id = nextId++;
  timers.set(id, { id, at: vnow + delay, fn, args, every: repeat ? delay : 0, seq: seq++ });
  return id;
}
g.setTimeout = (fn: any, ms?: any, ...args: any[]) => addTimer(fn, ms, args, false);
g.setInterval = (fn: any, ms?: any, ...args: any[]) => addTimer(fn, ms, args, true);
g.clearTimeout = g.clearInterval = (id: any) => void timers.delete(id);
g.setImmediate = (fn: any, ...args: any[]) => addTimer(fn, 0, args, false);
g.clearImmediate = g.clearTimeout;
g.__vnow = () => vnow;
/** Run every timer due at the earliest pending time. Returns false if none remain. */
g.__ticks = 0;
g.__runTimers = (): boolean => {
  g.__ticks++;
  if (timers.size === 0) {
    return false;
  }
  let at = Infinity;
  for (const t of timers.values()) {
    if (t.at < at) {
      at = t.at;
    }
  }
  vnow = at;
  const due = [...timers.values()].filter(t => t.at === at).sort((a, b) => a.seq - b.seq);
  for (const t of due) {
    if (!timers.has(t.id)) {
      continue;
    }
    if (t.every) {
      t.at += t.every;
      t.seq = seq++;
    } else {
      timers.delete(t.id);
    }
    try {
      t.fn(...t.args);
    } catch (e) {
      g.__reportError?.(e);
    }
  }
  return true;
};
g.queueMicrotask = (fn: any) => void Promise.resolve().then(fn);
g.requestAnimationFrame = (fn: any) => g.setTimeout(() => fn(vnow), 16);
g.cancelAnimationFrame = g.clearTimeout;
g.performance = { now: () => host.now(), mark() {}, measure() {} };

// ---- console: silent by default (the game logs a lot); errors go to the host ----
const fmt = (a: any[]) =>
  a.map(x => (typeof x === "string" ? x : x instanceof Error ? `${x.message}\n${x.stack}` : safeJson(x))).join(" ");
function safeJson(x: any) {
  try {
    return JSON.stringify(x)?.slice(0, 300);
  } catch {
    return String(x);
  }
}
g.__verbose = false;
g.__errorCount = 0;
const noop = () => {};
g.console = {
  log: (...a: any[]) => g.__verbose && host.print(fmt(a)),
  info: (...a: any[]) => g.__verbose && host.print(fmt(a)),
  debug: noop,
  trace: noop,
  warn: (...a: any[]) => g.__verbose && host.print("WARN " + fmt(a)),
  error: (...a: any[]) => {
    g.__errorCount++;
    if (g.__errorCount <= 20) {
      host.print("ERR " + fmt(a).slice(0, 400));
    }
  },
  group: noop,
  groupCollapsed: noop,
  groupEnd: noop,
  time: noop,
  timeEnd: noop,
  assert: noop,
  count: noop,
  table: noop,
  dir: noop,
};

// ---- generic DOM ----
class EventTargetShim {
  private _l: Record<string, Function[]> = {};
  addEventListener(t: string, f: Function) {
    (this._l[t] ??= []).push(f);
  }
  removeEventListener(t: string, f: Function) {
    this._l[t] = (this._l[t] ?? []).filter(x => x !== f);
  }
  dispatchEvent(e: any) {
    for (const f of this._l[e?.type] ?? []) {
      f.call(this, e);
    }
    return true;
  }
}
const mockContext: any = new Proxy(
  {
    canvas: null,
    font: "",
    measureText: () => ({ width: 0, actualBoundingBoxAscent: 0, actualBoundingBoxDescent: 0 }),
    getImageData: (_x: number, _y: number, w = 1, h = 1) => ({ data: new Uint8ClampedArray(Math.max(1, w * h * 4)), width: w, height: h }),
    createImageData: (w = 1, h = 1) => ({ data: new Uint8ClampedArray(Math.max(1, w * h * 4)), width: w, height: h }),
    createLinearGradient: () => ({ addColorStop() {} }),
    createPattern: () => ({}),
    getContextAttributes: () => ({}),
  },
  {
    get: (t: any, p) => (p in t ? t[p] : noop),
    set: (t: any, p, v) => ((t[p] = v), true),
  },
);
class Element extends EventTargetShim {
  tagName: string;
  style: any = {};
  children: any[] = [];
  childNodes: any[] = this.children;
  parentNode: any = null;
  attributes: any = {};
  classList = { add() {}, remove() {}, contains: () => false, toggle() {} };
  dataset: any = {};
  innerHTML = "";
  innerText = "";
  textContent = "";
  width = 1;
  height = 1;
  clientWidth = 1920;
  clientHeight = 1080;
  offsetWidth = 1920;
  offsetHeight = 1080;
  constructor(tag = "div") {
    super();
    this.tagName = tag.toUpperCase();
  }
  appendChild(c: any) {
    this.children.push(c);
    c.parentNode = this;
    return c;
  }
  removeChild(c: any) {
    this.children = this.children.filter(x => x !== c);
    return c;
  }
  insertBefore(c: any) {
    return this.appendChild(c);
  }
  append(...c: any[]) {
    c.forEach(x => this.appendChild(x));
  }
  remove() {}
  setAttribute(k: string, v: any) {
    this.attributes[k] = v;
  }
  getAttribute(k: string) {
    return this.attributes[k] ?? null;
  }
  removeAttribute(k: string) {
    delete this.attributes[k];
  }
  hasAttribute(k: string) {
    return k in this.attributes;
  }
  getBoundingClientRect() {
    return { left: 0, top: 0, right: 1920, bottom: 1080, width: 1920, height: 1080, x: 0, y: 0 };
  }
  querySelector() {
    return null;
  }
  querySelectorAll() {
    return [];
  }
  getElementsByTagName() {
    return [];
  }
  focus() {}
  blur() {}
  click() {}
  getContext() {
    return mockContext;
  }
  toDataURL() {
    return "data:,";
  }
  cloneNode() {
    return new Element(this.tagName);
  }
}
mockContext.canvas = new Element("canvas");
class HTMLCanvasElement extends Element {
  constructor() {
    super("canvas");
  }
}
class HTMLImageElement extends Element {
  src = "";
  complete = true;
  naturalWidth = 1;
  naturalHeight = 1;
  onload: any = null;
  onerror: any = null;
  constructor() {
    super("img");
  }
}
const body = new Element("body");
const head = new Element("head");
const documentElement = new Element("html");
let cookie = "";
const document: any = new EventTargetShim();
Object.assign(document, {
  body,
  head,
  documentElement,
  readyState: "complete",
  visibilityState: "visible",
  hidden: false,
  fonts: { add() {}, load: () => Promise.resolve([]), ready: Promise.resolve(), check: () => true },
  createElement: (t: string) =>
    t === "canvas" ? new HTMLCanvasElement() : t === "img" ? new HTMLImageElement() : new Element(t),
  createElementNS: (_ns: string, t: string) => new Element(t),
  createTextNode: (t: string) => ({ textContent: t }),
  getElementById: () => null,
  getElementsByTagName: () => [],
  getElementsByClassName: () => [],
  querySelector: () => null,
  querySelectorAll: () => [],
  hasFocus: () => true,
});
Object.defineProperty(document, "cookie", {
  get: () => cookie,
  set: (v: string) => {
    const kv = v.split(";")[0];
    const k = kv.split("=")[0];
    cookie = cookie
      .split("; ")
      .filter(x => x && x.split("=")[0] !== k)
      .concat(kv)
      .join("; ");
  },
});

Object.setPrototypeOf(g, EventTargetShim.prototype);
(g as any)._l = {};
g.window = g;
g.self = g;
g.global = g;
g.top = g;
g.parent = g;
g.document = document;
g.Element = Element;
g.HTMLElement = Element;
g.Node = Element;
g.HTMLCanvasElement = HTMLCanvasElement;
g.HTMLImageElement = HTMLImageElement;
g.HTMLVideoElement = class extends Element {};
g.HTMLDivElement = Element;
g.Image = HTMLImageElement;
g.EventTarget = EventTargetShim;
g.Event = class {
  type: string;
  constructor(type: string, init?: any) {
    this.type = type;
    Object.assign(this, init);
  }
  preventDefault() {}
  stopPropagation() {}
};
g.CustomEvent = class extends g.Event {};
g.KeyboardEvent = g.Event;
g.innerWidth = 1920;
g.innerHeight = 1080;
g.devicePixelRatio = 1;
g.screen = { width: 1920, height: 1080, orientation: { type: "landscape-primary", addEventListener() {} } };
g.location = { href: "http://localhost/", origin: "http://localhost", protocol: "http:", host: "localhost", hostname: "localhost", pathname: "/", search: "", hash: "", reload() {} };
g.history = { pushState() {}, replaceState() {} };
g.navigator = {
  userAgent: "Mozilla/5.0 (X11; Linux x86_64) QuickJS-bench",
  language: "en",
  languages: ["en"],
  platform: "Linux",
  maxTouchPoints: 0,
  onLine: false,
  getGamepads: () => [],
  vibrate: () => false,
};
g.matchMedia = () => ({ matches: false, addEventListener() {}, removeEventListener() {}, addListener() {} });
g.getComputedStyle = () => ({ getPropertyValue: () => "" });
g.focus = noop;
g.scrollTo = noop;
g.alert = noop;
g.open = noop;
g.FontFace = class {
  load() {
    return Promise.resolve(this);
  }
};
g.XMLHttpRequest = class {};
g.MutationObserver = class {
  observe() {}
  disconnect() {}
};
g.ResizeObserver = g.MutationObserver;
g.structuredClone ??= (v: any) => JSON.parse(JSON.stringify(v));

// ---- storage ----
function makeStorage() {
  let store: Record<string, string> = {};
  return {
    getItem: (k: string) => (k in store ? store[k] : null),
    setItem: (k: string, v: any) => void (store[k] = String(v)),
    removeItem: (k: string) => void delete store[k],
    clear: () => void (store = {}),
    key: (i: number) => Object.keys(store)[i] ?? null,
    get length() {
      return Object.keys(store).length;
    },
    hasOwnProperty: (k: string) => k in store,
  };
}
g.localStorage = makeStorage();
g.sessionStorage = makeStorage();

// ---- text / encoding ----
g.TextEncoder ??= class {
  encode(s: string) {
    const u = unescape(encodeURIComponent(s));
    const a = new Uint8Array(u.length);
    for (let i = 0; i < u.length; i++) {
      a[i] = u.charCodeAt(i);
    }
    return a;
  }
};
g.TextDecoder ??= class {
  decode(a?: Uint8Array) {
    if (!a) {
      return "";
    }
    let s = "";
    for (let i = 0; i < a.length; i++) {
      s += String.fromCharCode(a[i]);
    }
    try {
      return decodeURIComponent(escape(s));
    } catch {
      return s;
    }
  }
};
const B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
g.btoa ??= (s: string) => {
  let o = "";
  for (let i = 0; i < s.length; i += 3) {
    const a = s.charCodeAt(i), b = s.charCodeAt(i + 1), c = s.charCodeAt(i + 2);
    const n = (a << 16) | ((b || 0) << 8) | (c || 0);
    o += B64[(n >> 18) & 63] + B64[(n >> 12) & 63] + (Number.isNaN(b) ? "=" : B64[(n >> 6) & 63]) + (Number.isNaN(c) ? "=" : B64[n & 63]);
  }
  return o;
};
g.atob ??= (s: string) => {
  s = s.replace(/=+$/, "");
  let o = "";
  for (let i = 0; i < s.length; i += 4) {
    const n = (B64.indexOf(s[i]) << 18) | (B64.indexOf(s[i + 1]) << 12) | ((B64.indexOf(s[i + 2]) & 63) << 6) | (B64.indexOf(s[i + 3]) & 63);
    o += String.fromCharCode((n >> 16) & 255);
    if (s[i + 2] !== undefined) o += String.fromCharCode((n >> 8) & 255);
    if (s[i + 3] !== undefined) o += String.fromCharCode(n & 255);
  }
  return o;
};
g.crypto ??= {
  getRandomValues: (a: any) => {
    for (let i = 0; i < a.length; i++) a[i] = Math.floor(Math.random() * 256);
    return a;
  },
  randomUUID: () => "00000000-0000-4000-8000-" + Math.floor(Math.random() * 1e12).toString(16).padStart(12, "0"),
};

// ---- URL (QuickJS has none) ----
if (!g.URL) {
  class URLSearchParamsShim {
    private p: [string, string][] = [];
    constructor(q = "") {
      q.replace(/^\?/, "").split("&").filter(Boolean).forEach(kv => {
        const [k, v = ""] = kv.split("=");
        this.p.push([decodeURIComponent(k), decodeURIComponent(v)]);
      });
    }
    get(k: string) {
      return this.p.find(x => x[0] === k)?.[1] ?? null;
    }
    has(k: string) {
      return this.p.some(x => x[0] === k);
    }
    set(k: string, v: string) {
      this.p = this.p.filter(x => x[0] !== k);
      this.p.push([k, v]);
    }
    append(k: string, v: string) {
      this.p.push([k, v]);
    }
    toString() {
      return this.p.map(([k, v]) => `${encodeURIComponent(k)}=${encodeURIComponent(v)}`).join("&");
    }
  }
  g.URLSearchParams = URLSearchParamsShim;
  g.URL = class {
    href: string;
    pathname: string;
    search: string;
    searchParams: any;
    origin = "http://localhost";
    constructor(u: string) {
      this.href = u;
      const m = /^(?:[a-z]+:\/\/[^/]*)?([^?#]*)(\?[^#]*)?/.exec(u)!;
      this.pathname = m[1];
      this.search = m[2] ?? "";
      this.searchParams = new URLSearchParamsShim(this.search);
    }
    toString() {
      return this.href;
    }
    static createObjectURL() {
      return "blob:";
    }
    static revokeObjectURL() {}
  };
}
g.Blob ??= class {
  constructor(public parts: any[] = []) {}
  text() {
    return Promise.resolve(this.parts.join(""));
  }
};

// ---- fetch: serves bundled JSON (locales + a few data files), 404 otherwise ----
g.__files = {} as Record<string, unknown>;
g.Headers ??= class {
  get() {
    return null;
  }
  has() {
    return false;
  }
};
function response(ok: boolean, data: any) {
  return {
    ok,
    status: ok ? 200 : 404,
    statusText: ok ? "OK" : "Not Found",
    headers: new g.Headers(),
    json: () => Promise.resolve(data),
    text: () => Promise.resolve(typeof data === "string" ? data : JSON.stringify(data)),
    clone() {
      return this;
    },
  };
}
g.Response = class {
  static json(data: any, init?: any) {
    return response((init?.status ?? 200) < 400, data);
  }
};
g.fetch = (url: string) => {
  const u = String(url).split("?")[0].replace(/^\.?\//, "").replace(/^assets\//, "");
  const key = u.includes("battle-anims/") ? "battle-anims/tackle.json" : u;
  const hit = g.__files[key];
  return Promise.resolve(hit === undefined ? response(false, {}) : response(true, hit));
};

// ---- Intl (QuickJS has none): English-only minimal versions ----
if (!g.Intl) {
  const PluralRules = class {
    constructor(_l?: any, private o: any = {}) {}
    select(n: number) {
      if (this.o?.type === "ordinal") {
        const t = n % 10, h = n % 100;
        return t === 1 && h !== 11 ? "one" : t === 2 && h !== 12 ? "two" : t === 3 && h !== 13 ? "few" : "other";
      }
      return n === 1 ? "one" : "other";
    }
    resolvedOptions() {
      return { locale: "en", pluralCategories: ["one", "other"] };
    }
    static supportedLocalesOf(l: any) {
      return [].concat(l ?? []);
    }
  };
  const NumberFormat = class {
    constructor(_l?: any, private o: any = {}) {}
    format(n: number) {
      const d = this.o?.maximumFractionDigits;
      const s = d !== undefined ? Number(n).toFixed(d).replace(/\.?0+$/, "") : String(n);
      const [i, f] = s.split(".");
      return i.replace(/\B(?=(\d{3})+(?!\d))/g, ",") + (f ? "." + f : "");
    }
    formatToParts(n: number) {
      return [{ type: "integer", value: this.format(n) }];
    }
    resolvedOptions() {
      return { locale: "en" };
    }
  };
  const DateTimeFormat = class {
    format(d: any) {
      return new Date(d ?? Date.now()).toISOString();
    }
    formatToParts() {
      return [];
    }
    resolvedOptions() {
      return { locale: "en", timeZone: "UTC" };
    }
  };
  const Collator = class {
    compare(a: string, b: string) {
      return a < b ? -1 : a > b ? 1 : 0;
    }
  };
  const Locale = class {
    constructor(public baseName: string) {}
    get language() {
      return this.baseName.split("-")[0];
    }
    toString() {
      return this.baseName;
    }
  };
  g.Intl = {
    PluralRules,
    NumberFormat,
    DateTimeFormat,
    Collator,
    Locale,
    RelativeTimeFormat: DateTimeFormat,
    ListFormat: class {
      format(a: string[]) {
        return a.join(", ");
      }
    },
    Segmenter: class {
      segment(s: string) {
        return [...s].map((c, i) => ({ segment: c, index: i }));
      }
    },
    getCanonicalLocales: (l: any) => [].concat(l ?? []),
  };
}
g.process ??= { env: {}, listeners: () => [], removeListener() {}, on() {}, platform: "3ds", version: "", versions: {} };

export {};
