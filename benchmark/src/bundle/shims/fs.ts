export function readFileSync(): never {
  throw new Error("fs is not available in the benchmark host");
}
export default { readFileSync };
