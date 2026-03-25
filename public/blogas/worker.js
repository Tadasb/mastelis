self.onmessage = function(e) {
    const { start, count } = e.data;
    let nr = BigInt(start);
    const limit = nr + BigInt(count);
    
    // Process the batch
    while (nr < limit) {
        let n = nr;
        // The core Collatz logic
        while (n >= nr && n > 1n) {
            if (n & 1n) {
                n = (3n * n + 1n) >> 1n;
            } else {
                n >>= 1n;
            }
        }
        nr++;
    }
    
    // Report back that this batch is finished
    self.postMessage({ type: 'done', lastNr: nr - 1n, count: count });
};
