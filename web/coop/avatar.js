var CcAvatar = (function () {
  const options = {
    skin: [['Warm', '#bb9672'], ['Fair', '#e8c2a0'], ['Sand', '#cfaa80'], ['Copper', '#ab7556'], ['Umber', '#79513f'], ['Deep', '#4d352e']],
    hair: [['Ink', '#302a31'], ['Chestnut', '#604232'], ['Auburn', '#884b35'], ['Gold', '#ba9a5b'], ['Silver', '#b8b5aa'], ['Chalk', '#e2d9c6']],
    coat: [['Plum', '#604a68'], ['Moss', '#576447'], ['Rust', '#965340'], ['Ocean', '#405f72'], ['Ochre', '#ab8950'], ['Charcoal', '#42444a']],
    style: [['Cropped'], ['Swept'], ['Bob'], ['Crest'], ['Braided'], ['Long']],
    face: [['Square'], ['Long'], ['Broad'], ['Weathered']]
  };
  const fields = ['skin', 'hair', 'style', 'face', 'coat'];
  function normalize(value) {
    return Object.fromEntries(fields.map(key => [key,
      Number.isInteger(value?.[key]) && value[key] >= 0 && value[key] < options[key].length ? value[key] : 0]));
  }
  function pack(value) {
    const checked = normalize(value);
    return fields.reduce((packed, key, i) => packed | (checked[key] << (i * 3)), 0);
  }
  return {options, fields, normalize, pack};
})();
