// Arrays to store the last minute of data
const tempCh1Data = [];
const tempCh2Data = [];
const tempCh3Data = [];

$.fn.sparkline.defaults.common = {
    type: 'line',
    width: '100%',
    height: '100%',
    lineColor: '#59ff44',
    fillColor: 'transparent',
    dotColor: 'transparent',
    spotColor: 'transparent',
    minSpotColor: 'transparent',
    maxSpotColor: 'transparent',
    highlightSpotColor: 'transparent',
    highlightLineColor: 'transparent',
    chartRangeMin: 0,
};

// Use Server-Sent Events to fetch the current_value from the /stream endpoint
const eventSource = new EventSource('/stream');
eventSource.onmessage = function (event) {
    const tempCh1Element = document.getElementById('temp_ch1');
    const tempCh2Element = document.getElementById('temp_ch2');
    const tempCh3Element = document.getElementById('temp_ch3');
    if (event.data) {
        const data = JSON.parse(event.data);

        // Update the temperature values in the HTML
        tempCh1Element.innerHTML = `${data.tempCh1 !== null ? data.tempCh1 : "N/A"} &deg;F`;
        tempCh2Element.innerHTML = `${data.tempCh2 !== null ? data.tempCh2 : "N/A"} &deg;F`;
        tempCh3Element.innerHTML = `${data.tempCh3 !== null ? data.tempCh3 : "N/A"} &deg;F`;

        // Add new data to the arrays
        if (data.tempCh1 !== null) tempCh1Data.push(data.tempCh1);
        if (data.tempCh2 !== null) tempCh2Data.push(data.tempCh2);
        if (data.tempCh3 !== null) tempCh3Data.push(data.tempCh3);

        // Keep only the last 60 readings (assuming 1 reading per second)
        if (tempCh1Data.length > 60) tempCh1Data.shift();
        if (tempCh2Data.length > 60) tempCh2Data.shift();
        if (tempCh3Data.length > 60) tempCh3Data.shift();

        // Update sparklines
        $('#tempCh1Sparkline').sparkline(tempCh1Data);
        $('#tempCh2Sparkline').sparkline(tempCh2Data);
        $('#tempCh3Sparkline').sparkline(tempCh3Data);
    } else {
        tempCh1Element.innerHTML = '';
        tempCh2Element.innerHTML = '';
        tempCh3Element.innerHTML = '';
    }
};