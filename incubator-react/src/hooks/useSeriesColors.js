// Fixed 8-hue categorical order (validated: node scripts/validate_palette.js —
// all checks pass, worst adjacent CVD ΔE 24.2). Never cycled past 8 — a chart
// with more simultaneously visible series should cap selection instead of
// reusing or generating a 9th hue.
export const CATEGORICAL_PALETTE = [
    "#2a78d6", // blue
    "#1baf7a", // aqua
    "#eda100", // yellow
    "#008300", // green
    "#4a3aa7", // violet
    "#e34948", // red
    "#e87ba4", // magenta
    "#eb6834", // orange
];

export const MAX_VISIBLE_SERIES = CATEGORICAL_PALETTE.length;

// Stable slot assignment: a series keeps its color for as long as it stays
// visible. Newly-visible series take the lowest free slot; slots free up
// when a series is hidden. This is what keeps survivors' colors from
// repainting when the visible set changes (color follows the entity, not
// its rank). Pure function of (next visible keys, previous assignment) —
// called from the toggle event handler, not during render, since mutating
// persisted state (e.g. a ref) mid-render breaks React's rendering rules.
export function assignSeriesColors(visibleKeys, previousColors) {
    const usedSlots = new Set(
        visibleKeys
            .filter(key => previousColors[key] !== undefined)
            .map(key => CATEGORICAL_PALETTE.indexOf(previousColors[key]))
    );

    const next = {};
    for (const key of visibleKeys) {
        if (previousColors[key] !== undefined) {
            next[key] = previousColors[key];
            continue;
        }
        let slot = 0;
        while (usedSlots.has(slot)) slot++;
        next[key] = CATEGORICAL_PALETTE[slot];
        usedSlots.add(slot);
    }
    return next;
}
