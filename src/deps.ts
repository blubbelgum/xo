type DependencyMap = Map<string, Set<string>>;
type ReverseDependencyMap = Map<string, Set<string>>;
export const dependencies: DependencyMap = new Map();
export const reverseDependencies: ReverseDependencyMap = new Map();

export function trackDependencies(
  mdFile: string,
  layoutPath: string,
  partialPaths: string[],
) {
  // Clean up old dependencies more efficiently
  const existingDeps = dependencies.get(mdFile);
  if (existingDeps) {
    existingDeps.forEach((dep) => {
      reverseDependencies.get(dep)?.delete(mdFile);
    });
    dependencies.delete(mdFile);
  }

  // Update with new dependencies
  const allDeps = [layoutPath, ...partialPaths];
  dependencies.set(mdFile, new Set(allDeps));

  allDeps.forEach((dep) => {
    if (!reverseDependencies.has(dep)) {
      reverseDependencies.set(dep, new Set());
    }
    reverseDependencies.get(dep)!.add(mdFile);
  });
}
