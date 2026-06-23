setInterval(function () {
    $.getJSON('/api/dashboard', function (d) {
        $('#d-time').html('System Status &mdash; ' + d.time);
        $('#d-cpu-temp').html(d.cpu_temp + ' &deg;C');
        $('#d-free-heap').text(d.free_heap + ' bytes');
        $('#d-uptime').text(d.uptime);
        $('#d-wifi-rssi').text(d.wifi_rssi + ' dBm');
        $('#d-battery').html(d.battery_rows);
        $('#d-gnss').html(d.gnss_rows);
        $('#d-stats-all').text(d.stats_all);
        $('#d-stats-rf').text(d.stats_rf);
        $('#d-stations').html(d.last_stations_rows);
        $('#d-top-send').html(d.top_send_rows);
    });
}, 5000);
