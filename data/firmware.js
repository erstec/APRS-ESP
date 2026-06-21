function sub(obj) {
    var fileName = obj.value.split('\\');
    var name = fileName[fileName.length - 1];
    var label = document.getElementById('file-input');
    if (label) label.innerHTML = '   ' + name;
    var btn = document.getElementById('update_submit');
    if (btn) btn.value = name.toLowerCase().indexOf('littlefs') >= 0 ? 'Flash Filesystem' : 'Flash Firmware';
}

$('form').submit(function (e) {
    e.preventDefault();
    var data = new FormData($('#upload_form')[0]);
    $.ajax({
        url: '/update',
        type: 'POST',
        data: data,
        contentType: false,
        processData: false,
        xhr: function () {
            var xhr = new window.XMLHttpRequest();
            xhr.upload.addEventListener('progress', function (evt) {
                if (evt.lengthComputable) {
                    var pct = Math.round(evt.loaded / evt.total * 100);
                    $('#prg').html('Uploading: ' + pct + '%');
                    $('#bar').css('width', pct + '%');
                }
            }, false);
            return xhr;
        },
        success: function () {
            $('#prg').html('Done — rebooting…');
        },
        error: function () {
            $('#prg').html('Upload failed.');
        }
    });
});
