# Multi-edit capcode semantics

For multiple selected filters, `Address` / capcode uses explicit no-change semantics:

- Mixed capcodes display `Don't change` and remain editable.
- Leaving `Don't change` preserves every selected filter's existing capcode.
- Typing a valid capcode is an explicit override and applies it to all selected filters.
- When all selected filters already share the same capcode, that value is shown and remains editable.
- Existing per-filter address validation and field length limits still apply before saving.

This behavior is covered by the portable `filter_multi_edit` regression test and by the Win32 filter dialog implementation.
