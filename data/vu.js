$(document).ready(function () {
    var chart = {
        type: 'gauge',
        plotBorderWidth: 1,
        plotBackgroundColor: {
            linearGradient: { x1: 0, y1: 0, x2: 0, y2: 1 },
            stops: [
                [0, '#FFFFC6'],
                [0.3, '#FFFFFF'],
                [1, '#FFF4C6']
            ]
        },
        plotBackgroundImage: null,
        height: 200
    };
    var credits = { enabled: false };
    var title = { text: 'RX VU Meter' };
    var pane = [{
        startAngle: -45,
        endAngle: 45,
        background: null,
        center: ['50%', '145%'],
        size: 300
    }];
    var yAxis = [{
        min: -40,
        max: 1,
        minorTickPosition: 'outside',
        tickPosition: 'outside',
        labels: { rotation: 'auto', distance: 20 },
        plotBands: [
            { from: -10, to:   1, color: '#C02316', innerRadius: '100%', outerRadius: '105%' },
            { from: -20, to: -10, color: '#00C000', innerRadius: '100%', outerRadius: '105%' },
            { from: -30, to: -20, color: '#AFFF0F', innerRadius: '100%', outerRadius: '105%' },
            { from: -40, to: -30, color: '#C0A316', innerRadius: '100%', outerRadius: '105%' }
        ],
        pane: 0,
        title: { text: '<span style="font-size:12px">dBV</span>', y: -40 }
    }];
    var plotOptions = {
        gauge: {
            dataLabels: { enabled: false },
            dial: { radius: '100%' }
        }
    };
    var series = [{ data: [-40], yAxis: 0 }];

    var json = {};
    json.chart = chart;
    json.credits = credits;
    json.title = title;
    json.pane = pane;
    json.yAxis = yAxis;
    json.plotOptions = plotOptions;
    json.series = series;

    var chartFunction = function (chart) {
        var Vrms = 0;
        var dBV = -40;
        var active = 0;
        var raw = "";
        var timeStamp;

        setInterval(function () {
            if (chart.series) {
                var left = chart.series[0].points[0];
                $.getJSON("./realtime", function (json) {
                    active = parseInt(json.Active);
                    Vrms = parseFloat(json.mVrms) / 1000;
                    dBV = 20.0 * Math.log10(Vrms);
                    if (dBV < -40) dBV = -40;
                    raw = json.RAW;
                    timeStamp = Number(json.timeStamp);
                });
                if (active == 1) {
                    left.update(dBV, false);
                    chart.redraw();
                    var date = new Date(timeStamp * 1000);
                    var head = date + "[" + Vrms.toFixed(3) + "Vrms," + dBV.toFixed(1) + "dBV]\n";
                    document.getElementById("raw_txt").value += head + atob(raw) + "\n";
                }
            }
        }, 1000);
    };

    $('#vumeter').highcharts(json, chartFunction);
});
