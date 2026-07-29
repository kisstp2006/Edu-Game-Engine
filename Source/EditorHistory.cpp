#include "EditorHistory.h"

#include <algorithm>
#include <utility>

namespace EGE
{
	bool EditorDocumentState::HasSameDocumentData(
		const EditorDocumentState& other) const
	{
		return documentType == other.documentType &&
			documentId == other.documentId &&
			payload == other.payload;
	}

	std::size_t EditorDocumentState::GetMemoryUsage() const
	{
		return documentType.size() +
			documentId.size() +
			payload.size() +
			selectedObjects.size() * sizeof(uint);
	}

	std::size_t EditorHistoryEntry::GetMemoryUsage() const
	{
		return label.size() +
			before.GetMemoryUsage() +
			after.GetMemoryUsage();
	}

	EditorHistory::EditorHistory(
		std::size_t maximumEntries,
		std::size_t maximumBytes)
		: maximumEntries_(std::max<std::size_t>(maximumEntries, 1)),
		  maximumBytes_(std::max<std::size_t>(maximumBytes, 1))
	{
	}

	void EditorHistory::Clear()
	{
		entries_.clear();
		cursor_ = 0;
		memoryUsage_ = 0;
		Cancel();
	}

	void EditorHistory::Begin(
		std::string label,
		EditorDocumentState before)
	{
		if (transactionOpen_)
			return;
		transactionOpen_ = true;
		transactionLabel_ =
			label.empty() ? "Edit Scene" : std::move(label);
		transactionBefore_ = std::move(before);
	}

	bool EditorHistory::Commit(EditorDocumentState after)
	{
		if (!transactionOpen_)
			return false;

		std::string label = std::move(transactionLabel_);
		EditorDocumentState before =
			std::move(transactionBefore_);
		Cancel();
		return Push(
			std::move(label),
			std::move(before),
			std::move(after));
	}

	void EditorHistory::Cancel()
	{
		transactionOpen_ = false;
		transactionLabel_.clear();
		transactionBefore_ = {};
	}

	bool EditorHistory::Push(
		std::string label,
		EditorDocumentState before,
		EditorDocumentState after)
	{
		if (before.HasSameDocumentData(after))
			return false;

		RemoveRedoEntries();
		EditorHistoryEntry entry;
		entry.label = label.empty()
			? "Edit Scene"
			: std::move(label);
		entry.before = std::move(before);
		entry.after = std::move(after);
		memoryUsage_ += entry.GetMemoryUsage();
		entries_.push_back(std::move(entry));
		cursor_ = entries_.size();
		TrimToBudget();
		return true;
	}

	void EditorHistory::RebaseDocument(
		const std::string& documentType,
		const std::string& documentId)
	{
		for (EditorHistoryEntry& entry : entries_)
		{
			entry.before.documentType = documentType;
			entry.before.documentId = documentId;
			entry.after.documentType = documentType;
			entry.after.documentId = documentId;
		}
		if (transactionOpen_)
		{
			transactionBefore_.documentType = documentType;
			transactionBefore_.documentId = documentId;
		}

		memoryUsage_ = 0;
		for (const EditorHistoryEntry& entry : entries_)
			memoryUsage_ += entry.GetMemoryUsage();
		TrimToBudget();
	}

	const EditorDocumentState* EditorHistory::Undo()
	{
		if (!CanUndo())
			return nullptr;
		--cursor_;
		return &entries_[cursor_].before;
	}

	const EditorDocumentState* EditorHistory::Redo()
	{
		if (!CanRedo())
			return nullptr;
		const EditorDocumentState* state =
			&entries_[cursor_].after;
		++cursor_;
		return state;
	}

	bool EditorHistory::CanUndo() const
	{
		return !transactionOpen_ && cursor_ > 0;
	}

	bool EditorHistory::CanRedo() const
	{
		return !transactionOpen_ && cursor_ < entries_.size();
	}

	bool EditorHistory::HasOpenTransaction() const
	{
		return transactionOpen_;
	}

	const char* EditorHistory::GetUndoLabel() const
	{
		return CanUndo()
			? entries_[cursor_ - 1].label.c_str()
			: nullptr;
	}

	const char* EditorHistory::GetRedoLabel() const
	{
		return CanRedo()
			? entries_[cursor_].label.c_str()
			: nullptr;
	}

	std::size_t EditorHistory::GetEntryCount() const
	{
		return entries_.size();
	}

	std::size_t EditorHistory::GetMemoryUsage() const
	{
		return memoryUsage_;
	}

	void EditorHistory::TrimToBudget()
	{
		while (entries_.size() > 1 &&
			(entries_.size() > maximumEntries_ ||
			 memoryUsage_ > maximumBytes_))
		{
			memoryUsage_ -= entries_.front().GetMemoryUsage();
			entries_.erase(entries_.begin());
			if (cursor_ > 0)
				--cursor_;
		}
	}

	void EditorHistory::RemoveRedoEntries()
	{
		while (entries_.size() > cursor_)
		{
			memoryUsage_ -= entries_.back().GetMemoryUsage();
			entries_.pop_back();
		}
	}
}
