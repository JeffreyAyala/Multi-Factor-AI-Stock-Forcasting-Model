let chart;
let selectedAhead = 1;
let isFetching = false;
const REFRESH_INTERVAL_MS = 5000;

function setAhead(v) {
    selectedAhead = v;

    document.querySelectorAll('.horizon-buttons button').forEach(btn => {
        const txt = btn.innerText.replace('m','');
        if (parseInt(txt) === v) btn.classList.add('active');
        else btn.classList.remove('active');
    });

    loadData();
}

function loadData() {

    const t = document.getElementById('ticker').value.trim().toUpperCase();
    if (!t || isFetching) return;

    isFetching = true;
    const url = `http://localhost:5000/?ticker=${encodeURIComponent(t)}&ahead=${selectedAhead}`;

    fetch(url)
        .then(res => res.json())
        .then(d => {

            const setVal = (id, v) => {
                document.getElementById(id).innerText =
                    (typeof v === "number" && isFinite(v)) ? v.toFixed(2) : "-";
            };

            // Basic + Predictions
            setVal("current", d.current);
            setVal("nowcast", d.nowcast);
            setVal("prediction", d.prediction);

            // Technical Indicators
            setVal("sma20", d.sma20);
            setVal("ema20", d.ema20);
            setVal("rsi14", d.rsi14);
            setVal("rsiDiv", d.rsiDiv);
            setVal("magZone", d.magZone);

            // Advanced AI Signals
            setVal("trend", d.trend);
            setVal("volPressure", d.volPressure);

            // Multi-Factor ML Features
            setVal("newsSent", d.news_sentiment);
            setVal("pcr", d.pcr);
            setVal("vix", d.vix);

            // Session Flags
            document.getElementById("london").innerText = d.is_london ? "Yes" : "No";
            document.getElementById("ny").innerText     = d.is_newyork ? "Yes" : "No";
            document.getElementById("powerHour").innerText = d.is_powerhour ? "Yes" : "No";

            // Label update
            document.getElementById("prediction-label").innerText =
                `Prediction (+${selectedAhead} min):`;

            // Chart Update
            const closes = d.closes || [];
            const labels = closes.map((_, i) => i);

            if (chart) {
                chart.data.labels = labels;
                chart.data.datasets[0].data = closes;
                chart.update();
            } else {
                const ctx = document.getElementById('chart').getContext('2d');
                chart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels,
                        datasets: [{
                            label: 'Close Price',
                            data: closes,
                            borderWidth: 2,
                            borderColor: '#3b82f6',
                            pointRadius: 0,
                            pointHitRadius: 6,
                            fill: false,
                            tension: 0.25
                        }]
                    },
                    options: {
                        responsive: true,
                        scales: {
                            x: { ticks: { color: '#9ca3af'} },
                            y: { ticks: { color: '#9ca3af'} }
                        },
                        plugins: {
                            legend: { labels: { color: '#e5e7eb' } }
                        }
                    }
                });
            }
        })
        .finally(() => {
            isFetching = false;
        });
}

document.addEventListener("DOMContentLoaded", () => {
    setAhead(1);
    setInterval(loadData, REFRESH_INTERVAL_MS);
});


