import { useEffect, useState } from "react";
import { Plus, X } from "lucide-react";

export function AssetTagEditor({ tags, disabled, maximumCount = 16, maximumLength = 32, onApply }: {
  tags: string[];
  disabled: boolean;
  maximumCount?: number;
  maximumLength?: number;
  onApply: (tags: string[]) => Promise<boolean>;
}) {
  const [draft, setDraft] = useState("");
  const [error, setError] = useState("");
  useEffect(() => { setDraft(""); setError(""); }, [tags]);

  const add = async () => {
    const value = draft.trim();
    if (!value) return;
    if (value !== draft) { setError("Tags cannot start or end with spaces."); return; }
    if (value.length > maximumLength) { setError(`Tags can be up to ${maximumLength} characters.`); return; }
    if (tags.includes(value)) { setError("That tag is already assigned."); return; }
    if (tags.length >= maximumCount) { setError(`Assets can have up to ${maximumCount} tags.`); return; }
    if (await onApply([...tags, value])) { setDraft(""); setError(""); }
  };

  return <div className="asset-tag-editor">
    <span className="asset-tag-editor-title">Tags</span>
    {tags.length > 0 && <div className="asset-tag-list">
      {tags.map(tag => <span className="asset-tag" key={tag}>{tag}<button type="button"
        title={`Remove ${tag}`} aria-label={`Remove ${tag}`} disabled={disabled}
        onClick={() => void onApply(tags.filter(item => item !== tag))}><X size={12} /></button></span>)}
    </div>}
    <div className="asset-tag-entry">
      <input type="text" value={draft} maxLength={maximumLength} disabled={disabled || tags.length >= maximumCount}
        placeholder="Add a tag" aria-label="New asset tag" onChange={event => { setDraft(event.target.value); setError(""); }}
        onKeyDown={event => { if (event.key === "Enter") { event.preventDefault(); void add(); } }} />
      <button className="icon-button" type="button" title="Add tag" aria-label="Add tag"
        disabled={disabled || !draft || tags.length >= maximumCount} onClick={() => void add()}><Plus size={15} /></button>
    </div>
    {error && <small className="error-text">{error}</small>}
  </div>;
}
