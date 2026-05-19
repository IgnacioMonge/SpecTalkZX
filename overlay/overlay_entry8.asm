;; overlay_entry8.asm -- IRC bookmark selector storage.

SECTION code_user

EXTERN _bookmarks_render_ovl
EXTERN _bookmarks_rows_ovl
EXTERN _bookmarks_list_ovl
EXTERN _bookmarks_delete_ovl
EXTERN _bookmarks_cursor_ovl

    dw 5
    dw _bookmarks_render_ovl
    dw _bookmarks_rows_ovl
    dw _bookmarks_list_ovl
    dw _bookmarks_delete_ovl
    dw _bookmarks_cursor_ovl
