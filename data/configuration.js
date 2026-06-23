function sub(obj) {
    var fileName = obj.value.split('\\');
    var label = document.getElementById('file-input');
    if (label) label.innerHTML = '   ' + fileName[fileName.length - 1];
}

$('#upload_form').submit(function (e) {
    e.preventDefault();
    var data = new FormData($('#upload_form')[0]);
    $.ajax({
        url: '/updateconfig',
        type: 'POST',
        data: data,
        contentType: false,
        processData: false,
        xhr: function () {
            var xhr = new window.XMLHttpRequest();
            xhr.upload.addEventListener('progress', function (evt) {
                if (evt.lengthComputable) {
                    var pct = Math.round(evt.loaded / evt.total * 100);
                    $('#prg').html('progress: ' + pct + '%');
                    $('#bar').css('width', pct + '%');
                }
            }, false);
            return xhr;
        },
        success: function () {
            $('#prg').html('<b>Config restored. Please reboot the device for changes to take effect.</b>');
            $('#bar').css('width', '100%');
        },
        error: function () {}
    });
});
