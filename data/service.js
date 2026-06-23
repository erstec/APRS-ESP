// ── Passcode calculator ──────────────────────────────────────────────────────
function calcPasscode() {
  var call = document.getElementById('myCall').value.toUpperCase().split('-')[0];
  if (!call) return;
  var hash = 0x73e2;
  for (var i = 0; i < call.length; i += 2) {
    hash ^= call.charCodeAt(i) << 8;
    if (i + 1 < call.length) hash ^= call.charCodeAt(i + 1);
  }
  document.getElementById('myPasscode').value = hash & 0x7FFF;
}

// ── APRS-IS server presets ───────────────────────────────────────────────────
var presets = [
  'rotate.aprs2.net','euro.aprs2.net','noam.aprs2.net',
  'soam.aprs2.net','asia.aprs2.net','aunz.aprs2.net'
];

function onServerPreset(sel) {
  var manual = document.getElementById('aprsHostManual');
  if (sel.value === '') {
    manual.style.display = '';
    manual.name = 'aprsHost';
    sel.removeAttribute('name');
  } else {
    manual.style.display = 'none';
    manual.removeAttribute('name');
    sel.name = 'aprsHost';
  }
}

(function initServer() {
  var sel = document.getElementById('aprsHostPreset');
  var manual = document.getElementById('aprsHostManual');
  var cur = manual.value;
  if (presets.indexOf(cur) >= 0) {
    sel.value = cur;
    sel.name = 'aprsHost';
    manual.removeAttribute('name');
    manual.style.display = 'none';
  } else {
    sel.value = '';
    sel.removeAttribute('name');
    manual.name = 'aprsHost';
    manual.style.display = '';
  }
})();

// ── Filter builder ───────────────────────────────────────────────────────────
var filterTokens = [];

var filterDefs = {
  m: { label: 'My Range (m)', fields: [
    { id:'dist', label:'km', placeholder:'150', style:'width:70px' }
  ], build: function(v) { return v.dist ? 'm/'+v.dist : null; }},
  r: { label: 'Range (r)', fields: [
    { id:'lat',  label:'Lat', placeholder:'54.68' },
    { id:'lon',  label:'Lon', placeholder:'25.24' },
    { id:'dist', label:'km',  placeholder:'50', style:'width:70px' }
  ], build: function(v) { return (v.lat&&v.lon&&v.dist) ? 'r/'+v.lat+'/'+v.lon+'/'+v.dist : null; }},
  p: { label: 'Prefix (p)', fields: [
    { id:'pfx', label:'Prefix', placeholder:'LY' }
  ], build: function(v) { return v.pfx ? 'p/'+v.pfx.toUpperCase() : null; }},
  b: { label: 'Buddy (b)', fields: [
    { id:'calls', label:'Callsigns', placeholder:'LY3PH LY2EN' }
  ], build: function(v) { return v.calls ? 'b/'+v.calls.toUpperCase().trim().replace(/\s+/g,'/') : null; }},
  t: { label: 'Type (t)', fields: [], build: function() {
    var types = 'poimqstunw'.split('');
    var s = types.filter(function(c){ var el=document.getElementById('ft_'+c); return el&&el.checked; }).join('');
    return s ? 't/'+s : null;
  }}
};

var typeLabels = {p:'pos',o:'obj',i:'item',m:'msg',q:'query',s:'status',t:'tlm',u:'user',n:'nws',w:'wx'};

function onFilterType() {
  var type = document.getElementById('filterType').value;
  var def = filterDefs[type];
  var html = '';
  if (type === 't') {
    html = Object.keys(typeLabels).map(function(c) {
      return '<label style="display:inline-flex;align-items:center;gap:3px;margin-right:6px">'
        +'<input type="checkbox" id="ft_'+c+'"> '+typeLabels[c]+'</label>';
    }).join('');
  } else {
    def.fields.forEach(function(f) {
      html += '<label style="display:inline-flex;align-items:center;gap:4px;margin-right:6px">'+f.label
        +' <input type="text" id="fp_'+f.id+'" placeholder="'+f.placeholder
        +'" style="width:80px'+(f.style?';'+f.style:'')+'"></label>';
    });
  }
  document.getElementById('filterParams').innerHTML = html;
}

function addFilterToken() {
  var type = document.getElementById('filterType').value;
  var def = filterDefs[type];
  var vals = {};
  def.fields.forEach(function(f) {
    var el = document.getElementById('fp_'+f.id);
    if (el) vals[f.id] = el.value.trim();
  });
  var token = def.build(vals);
  if (!token) return;
  filterTokens.push(token);
  renderTokens();
}

function renderTokens() {
  var el = document.getElementById('filterTokens');
  if (!filterTokens.length) { el.innerHTML = '<span style="color:var(--text-dim);font-size:0.85em">no tokens</span>'; return; }
  el.innerHTML = filterTokens.map(function(t, i) {
    return '<span style="background:var(--surface2);border:1px solid var(--border);border-radius:4px;padding:2px 8px;font-size:0.85em;display:inline-flex;align-items:center;gap:6px">'
      +t+'<button type="button" onclick="removeToken('+i+')" style="background:none;border:none;color:var(--danger);cursor:pointer;padding:0;line-height:1">&times;</button></span>';
  }).join('');
}

function removeToken(i) {
  filterTokens.splice(i, 1);
  renderTokens();
}

function applyFilter() {
  document.getElementById('aprsFilter').value = filterTokens.join(' ');
}

(function initFilter() {
  onFilterType();
  var cur = (document.getElementById('aprsFilter').value || '').trim();
  filterTokens = cur ? cur.split(/\s+/) : [];
  renderTokens();
})();
