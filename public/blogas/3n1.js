const e = (id) => document.getElementById(id)
const t = (el, text = null) => text !== null ? (el.innerText = text) : el.innerText
const dg = (key) => window.localStorage.getItem(key)
const ds = (key, val) => { try { window.localStorage.setItem(key, val.toString()); } catch(e){} }

const FRONTIER = 295147905179352825856n // 2^68
const TARGET = 590295810358705651712n   // 2^69

let initialNr = BigInt(dg('initialNr') || FRONTIER)
let currentNr = BigInt(dg('nr') || e('skaicius').value || FRONTIER)
let totalSeconds = parseInt(dg('totalSeconds') || 0)

let workers = []
let running = false
let stopping = false
let activeWorkerCount = 0
let timerInterval = null

const updateStats = () => {
    t(e('tikrinamas'), currentNr.toString())
    t(e('aktyvus'), activeWorkerCount.toString())
    
    const diff = currentNr - initialNr
    const remaining = TARGET - currentNr
    
    t(e('papildomai'), diff.toString())
    t(e('laikas'), totalSeconds.toString())
    t(e('like'), remaining > 0n ? remaining.toString() : "Pasiekta!")
    
    if (totalSeconds > 0) {
        const speed = diff / BigInt(totalSeconds)
        t(e('greitis'), speed.toString())
    }
}

const handleWorkerMessage = (worker, msg) => {
    const { lastNr } = msg.data
    const lastNumBig = BigInt(lastNr)

    // Update progress ONLY at the end of a batch
    if (lastNumBig > currentNr) {
        currentNr = lastNumBig
    }

    if (running && !stopping) {
        // Send the next batch
        const batchSize = BigInt(e('paketas').value || 1000000)
        worker.postMessage({ start: currentNr.toString(), count: batchSize.toString() })
        currentNr += batchSize
    } else {
        // Stop this specific worker gracefully
        worker.terminate()
        activeWorkerCount--
        updateStats()
        
        // If all workers are finished, fully stop the UI
        if (activeWorkerCount === 0) {
            finishStopping()
        }
    }
}

const doMath = () => {
    if (running || stopping) {
        initiateStop()
        return
    }

    // Sync current values from inputs
    try {
        const inputVal = e('skaicius').value
        if (inputVal) {
            const inputBig = BigInt(inputVal)
            if (inputBig !== currentNr) {
                currentNr = inputBig
                initialNr = currentNr
                totalSeconds = 0
                ds('initialNr', initialNr)
                ds('totalSeconds', 0)
            }
        }
    } catch(err) { console.error(err) }

    running = true
    stopping = false
    t(e('mygtukas'), 'Sustabdyti')
    
    const numWorkers = parseInt(e('darbuotojai').value || 4)
    const batchSize = BigInt(e('paketas').value || 1000000)
    
    activeWorkerCount = numWorkers
    for (let i = 0; i < numWorkers; i++) {
        const w = new Worker('worker.js')
        w.onmessage = (msg) => handleWorkerMessage(w, msg)
        w.postMessage({ start: currentNr.toString(), count: batchSize.toString() })
        currentNr += batchSize
        workers.push(w)
    }

    timerInterval = setInterval(() => {
        totalSeconds++
        updateStats()
        ds('nr', currentNr)
        ds('totalSeconds', totalSeconds)
    }, 1000)
}

const initiateStop = () => {
    if (stopping) return
    stopping = true
    t(e('mygtukas'), 'Stabdoma...')
}

const finishStopping = () => {
    running = false
    stopping = false
    clearInterval(timerInterval)
    workers = []
    
    t(e('mygtukas'), 'Skaičiuoti')
    e('skaicius').value = currentNr.toString()
    ds('nr', currentNr)
    ds('totalSeconds', totalSeconds)
    updateStats()
}

// Initial Sync
const savedNr = dg('nr')
if (savedNr) currentNr = BigInt(savedNr)
const savedInit = dg('initialNr')
if (savedInit) initialNr = BigInt(savedInit)

updateStats()
e('skaicius').value = currentNr.toString()
