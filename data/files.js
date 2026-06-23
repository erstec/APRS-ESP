var currentEditPath = '';
var editor = null;

function cmMode(path) {
    var ext = path.split('.').pop().toLowerCase();
    if (ext === 'js')                        return { name: 'javascript' };
    if (ext === 'json')                      return { name: 'javascript', json: true };
    if (ext === 'css')                       return 'css';
    if (ext === 'html' || ext === 'htm')     return 'htmlmixed';
    if (ext === 'xml')                       return 'xml';
    return null;
}

function fmtSize(bytes) {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1048576) return (bytes / 1024).toFixed(1) + ' KB';
    return (bytes / 1048576).toFixed(2) + ' MB';
}

function loadFiles() {
    $.getJSON('/api/files', function (files) {
        files.sort(function (a, b) { return a.name.localeCompare(b.name); });
        var html = '';
        files.forEach(function (f) {
            var enc  = encodeURIComponent(f.path);
            var safe = f.path.replace(/&/g,'&amp;').replace(/"/g,'&quot;');
            var acts = '<a href="/api/file?path=' + enc + '&dl=1" download="' + f.name.replace(/"/g,'') + '">&#x2B73; Download</a>';
            if (f.isText) {
                acts += ' &nbsp;<a href="#" class="js-edit" data-path="' + safe + '">&#x270E; Edit</a>';
            }
            acts += ' &nbsp;<a href="#" class="js-del" data-path="' + safe + '" style="color:#e55">&#x2715; Delete</a>';
            html += '<tr>'
                 + '<td>' + f.name + (f.isDir ? '/' : '') + '</td>'
                 + '<td align="right" style="opacity:0.6">' + (f.isDir ? '&mdash;' : fmtSize(f.size)) + '</td>'
                 + '<td align="right" style="white-space:nowrap">' + acts + '</td>'
                 + '</tr>';
        });
        if (!html) html = '<tr><td colspan="3" style="opacity:0.5">No files found</td></tr>';
        $('#file-list').html(html);
    }).fail(function () {
        $('#file-list').html('<tr><td colspan="3" style="color:#e55">Failed to load file list</td></tr>');
    });
}

function uploadFiles() {
    var input = document.getElementById('upload-input');
    if (!input.files.length) { $('#upload-status').text('No file selected.'); return; }
    var queue = Array.from(input.files);
    var done  = 0;

    function uploadNext() {
        if (!queue.length) {
            $('#upload-status').text('Uploaded ' + done + ' file(s).');
            loadFiles();
            input.value = '';
            return;
        }
        var file = queue.shift();
        $('#upload-status').text('Uploading ' + file.name + '…');
        var data = new FormData();
        data.append('file', file, file.name);
        $.ajax({
            url: '/api/upload',
            type: 'POST',
            data: data,
            processData: false,
            contentType: false,
            success: function () { done++; uploadNext(); },
            error:   function () { $('#upload-status').text('Error uploading ' + file.name); }
        });
    }
    uploadNext();
}

function editFile(path) {
    currentEditPath = path;
    $('#edit-path').text(path);
    $('#save-status').text('');
    $('#edit-modal').show();

    if (!editor) {
        editor = CodeMirror(document.getElementById('edit-content'), {
            lineNumbers:  true,
            theme:        'dracula',
            indentUnit:   2,
            tabSize:      2,
            lineWrapping: false,
            readOnly:     false,
            value:        'Loading…'
        });
        editor.setSize('100%', '400px');
    }

    editor.setOption('mode', cmMode(path));
    editor.setValue('Loading…');

    $.get('/api/file?path=' + encodeURIComponent(path), function (content) {
        editor.setValue(content);
        editor.clearHistory();
    }, 'text').fail(function () {
        editor.setValue('// Error loading file');
    });
}

function saveFile() {
    if (!currentEditPath || !editor) return;
    $('#save-status').text('Saving…');
    $.post('/api/file', { path: currentEditPath, content: editor.getValue() })
        .done(function () { $('#save-status').text('Saved.'); })
        .fail(function () { $('#save-status').text('Save failed!'); });
}

function closeEditor() {
    $('#edit-modal').hide();
    currentEditPath = '';
}

function deleteFile(path) {
    if (!confirm('Delete ' + path + '?')) return;
    $.post('/api/delete', { path: path })
        .done(function () { loadFiles(); })
        .fail(function () { alert('Delete failed.'); });
}

$(document).ready(function () {
    loadFiles();

    $(document).on('click', '.js-edit', function (e) {
        e.preventDefault();
        editFile($(this).data('path'));
    });

    $(document).on('click', '.js-del', function (e) {
        e.preventDefault();
        deleteFile($(this).data('path'));
    });
});
