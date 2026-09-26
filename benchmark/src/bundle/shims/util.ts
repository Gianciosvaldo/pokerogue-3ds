export function inspect(v: any): string {
  try {
    return typeof v === "string" ? v : JSON.stringify(v);
  } catch {
    return String(v);
  }
}
export default { inspect };
export type InspectOptions = any;
