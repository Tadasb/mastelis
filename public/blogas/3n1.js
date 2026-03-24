const e = (id) => document.getElementById(id)
const t = (e, text = null) => text !== null ? (e.innerText = text) : e.innerText
dg = (key) => window.localStorage.getItem(key)
ds = (key, val) => window.localStorage.setItem(key, val)
n = dg('nr') || e('skaicius').value
nr = dg('nr') || e('skaicius').value
check = true

doMath = () => {
    setInterval(() => t(e('tikrinamas'), n), 1000)

    while (check) {
        n = nr
        while (n != 1 && n >= nr) {
            if (n % 2 == 0) {
                n = n / 2;
            } else {
                n = 3 * n + 1;
            }
        }
        nr++

        t(e('mygtukas'), 'Sustabdyti')
    }

}

stopMath = () => {
    check = false
    t(e('mygtukas'), 'Skaičiuoti')
    t(e('tikrinamas'), nr)
    ds('nr', nr)
}