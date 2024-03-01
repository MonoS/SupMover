meta:
  id: sup
  title: PGS/Sup Subtitle
  file-extension: sup
  endian: be
doc: |
  PGS/Sup Subtitle
doc-ref:
  - https://blog.thescorpius.com/index.php/2017/07/15/presentation-graphic-stream-sup-files-bluray-subtitle-format/
  - https://github.com/SubtitleEdit/subtitleedit/tree/main/src/libse/BluRaySup
seq:
  - id: segment
    type: sup_segment
    repeat: eos
types:
  sup_segment:
    seq:
      - id: magic
        contents: 'PG'
      - id: pts
        type: u4
      - id: dts
        type: u4
      - id: segment_type
        type: u1
      - id: segment_size
        type: u2
      - id: segment_body
        type:
          switch-on: segment_type
          cases:
            0x14: pds
            0x15: ods
            0x16: pcs
            0x17: wds
            0x80: end
        size: segment_size
  pds:
    seq:
      - id: palette_id
        type: u1
      - id: palette_version_number
        type: u1
      - id: palette
        type: palette_type
        repeat: expr
        repeat-expr: (_parent.segment_size -2) / 5
        
  ods:
    seq:
      - id: object_id
        type: u2
      - id: object_version_number
        type: u1
      - id: last_in_sequence_flag
        type: u1
      - id: object_data_length
        type: b24
      - id: width
        type: u2
      - id: height
        type: u2
      - id: object_data
        size: object_data_length - 4
  pcs:
    seq:
      - id: width
        type: u2
      - id: height
        type: u2
      - id: frame_rate
        type: u1
      - id: composition_number
        type: u2
      - id: composition_state
        type: u1
      - id: palette_upd_flag
        type: u1
      - id: palette_id
        type: u1
      - id: num_composition_object
        type: u1
      - id: composition_object
        type: composition_object_type
        repeat: expr
        repeat-expr: num_composition_object
  wds:
    seq:
      - id: number_of_windows
        type: u1
      - id: windows_object
        type: windows_object_type
        repeat: expr
        repeat-expr: number_of_windows
  end:
    seq: []
  
  composition_object_type:
    seq:
      - id: object_id
        type: u2
      - id: window_id
        type: u1
      - id: object_cropped_flag
        type: u1
      - id: object_hor_pos
        type: u2
      - id: object_ver_pos
        type: u2
      - id: obj_crop_hor_pos
        type: u2
        if: object_cropped_flag & 0x80 != 0
      - id: obj_crop_ver_pos
        type: u2
        if: object_cropped_flag & 0x80 != 0
      - id: obj_crop_width
        type: u2
        if: object_cropped_flag & 0x80 != 0
      - id: obj_crop_height
        type: u2
        if: object_cropped_flag & 0x80 != 0

  windows_object_type:
    seq:
      - id: window_id
        type: u1
      - id: windows_hor_pos
        type: u2
      - id: windows_ver_pos
        type: u2
      - id: windows_width
        type: u2
      - id: windows_height
        type: u2
        
  palette_type:
    seq:
      - id: palette_entry_id
        type: u1
      - id: palette_y
        type: u1
      - id: palette_cr
        type: u1
      - id: palette_cb
        type: u1
      - id: palette_a
        type: u1