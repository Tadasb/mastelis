const e = (id) => document.getElementById(id)
const t = (el, text = null) => text !== null ? (el.innerText = text) : el.innerText
const dg = (key) => window.localStorage.getItem(key)
const ds = (key, val) => {
    try {
        window.localStorage.setItem(key, val.toString());
    } catch (err) {
        console.error("Storage failed", err);
    }
}

const FRONTIER = 295147905179352825856n // 2^68
const TARGET = 590295810358705651712n   // 2^69

let initialNr = BigInt(dg('initialNr') || FRONTIER)
let nr = BigInt(dg('nr') || e('skaicius').value || FRONTIER)
let totalSeconds = parseInt(dg('totalSeconds') || 0)

let running = false
let lastUpdate = 0
let lastSecondTick = 0

const updateStats = () => {
    const finalVal = nr.toString()
    t(e('tikrinamas'), finalVal)
    
    // Calculate stats
    const diff = nr - initialNr
    const remaining = TARGET - nr
    
    t(e('papildomai'), diff > 0n ? diff.toString() : "0")
    t(e('laikas'), totalSeconds.toString())
    t(e('like'), remaining > 0n ? remaining.toString() : "Pasiekta!")
    
    // Average speed (numbers per second)
    if (totalSeconds > 0) {
        const speed = diff / BigInt(totalSeconds)
        t(e('greitis'), speed.toString())
    }
}

const step = () => {
    if (!running) return

    let startTime = performance.now()
    // Process for ~25ms per frame
    while (performance.now() - startTime < 25) {
        let n = nr
        while (n >= nr && n > 1n) {
            if (n & 1n) {
                n = (3n * n + 1n) >> 1n
            } else {
                n >>= 1n
            }
        }
        nr++
    }

    const now = performance.now()
    
    // Every second, increment the total time and save progress
    if (now - lastSecondTick >= 1000) {
        totalSeconds++
        lastSecondTick = now
        ds('totalSeconds', totalSeconds)
        ds('nr', nr)
        updateStats()
    }

    requestAnimationFrame(step)
}

const doMath = () => {
    if (running) {
        stopMath()
        return
    }
    
    try {
        const inputVal = e('skaicius').value
        if (inputVal) {
            const inputBig = BigInt(inputVal)
            // If user manually changed the input, reset the "initial" point for stats
            if (inputBig !== nr) {
                nr = inputBig
                initialNr = nr
                totalSeconds = 0
                ds('initialNr', initialNr)
                ds('totalSeconds', totalSeconds)
            }
        }
    } catch (err) {
        console.error("Invalid input", err)
    }
    
    running = true
    lastSecondTick = performance.now()
    t(e('mygtukas'), 'Sustabdyti')
    step()
}

const stopMath = () => {
    running = false
    t(e('mygtukas'), 'Skaičiuoti')
    
    updateStats()
    e('skaicius').value = nr.toString()
    ds('nr', nr)
    ds('totalSeconds', totalSeconds)
}

// Initial UI sync
const savedNr = dg('nr')
if (savedNr) {
    nr = BigInt(savedNr)
}
const savedInitial = dg('initialNr')
if (savedInitial) {
    initialNr = BigInt(savedInitial)
}

updateStats()
e('skaicius').value = nr.toString()
