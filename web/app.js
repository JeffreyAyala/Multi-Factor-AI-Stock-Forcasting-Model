let chart;
let selectedAhead = 1; // minutes ahead

function setAhead(v){
    selectedAhead = v;

    // Highlight active button
    document.querySelectorAll('.horizon-buttons button').forEach(btn => {
        const txt = btn.innerText.replace('m','');
        const val = parseInt(txt, 10);
        if(val === v) btn.classList.add('active');
        else btn.classList.remove('active');
    });

    loadData();
}

function loadData(){
    const t = document.getElementById('ticker').value.trim().toUpperCase();
    if(!t) return;

    const url = 'http://localhost:5000/?ticker=' +
                encodeURIComponent(t) +
                '&ahead=' + selectedAhead;

    fetch(url)
    .then(r => r.json())
    .then(d => {
        document.getElementById('current').innerText    = d.current.toFixed(2);
        document.getElementById('prediction').innerText = d.prediction.toFixed(2);
        document.getElementById('sma20').innerText      = d.sma20.toFixed(2);
        document.getElementById('ema20').innerText      = d.ema20.toFixed(2);
        document.getElementById('rsi14').innerText      = d.rsi14.toFixed(2);

        document.getElementById('prediction-label').innerText =
            `Prediction (+${selectedAhead} min):`;

        const closes = d.closes || [];
        const labels = closes.map((_, i) => i);

        if(chart){
            chart.data.labels = labels;
            chart.data.datasets[0].data = closes;
            chart.update();
        } else {
            const ctx = document.getElementById('chart').getContext('2d');
            chart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: labels,
                    datasets: [{
                        label: 'Close Price',
                        data: closes,
                        borderWidth: 2,
                        borderColor: '#3b82f6',
                        pointRadius: 0,          // cleaner trend line
                        pointHitRadius: 6,
                        fill: false,
                        tension: 0.25            // smooth curve
                    }]
                },
                options: {
                    responsive: true,
                    scales: {
                        x: {
                            display: true,
                            title: { display: true, text: 'Bar index' },
                            ticks: { color: '#9ca3af' }
                        },
                        y: {
                            display: true,
                            title: { display: true, text: 'Price' },
                            ticks: { color: '#9ca3af' }
                        }
                    },
                    plugins: {
                        legend: { labels: { color: '#e5e7eb' } }
                    }
                }
            });
        }
    })
    .catch(e => alert('Error: ' + e));
}

document.addEventListener('DOMContentLoaded', () => {
    setAhead(1);
});


