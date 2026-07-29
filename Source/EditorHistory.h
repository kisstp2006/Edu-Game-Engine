#pragma once

#include "Globals.h"

#include <cstddef>
#include <string>
#include <vector>

namespace EGE
{
	struct EditorDocumentState
	{
		std::string documentType = "Scene";
		std::string documentId;
		std::string payload;
		std::vector<uint> selectedObjects;
		uint primaryObject = 0;

		[[nodiscard]] bool HasSameDocumentData(
			const EditorDocumentState& other) const;
		[[nodiscard]] std::size_t GetMemoryUsage() const;
	};

	struct EditorHistoryEntry
	{
		std::string label;
		EditorDocumentState before;
		EditorDocumentState after;

		[[nodiscard]] std::size_t GetMemoryUsage() const;
	};

	class EditorHistory final
	{
	public:
		explicit EditorHistory(
			std::size_t maximumEntries = 128,
			std::size_t maximumBytes = 128 * 1024 * 1024);

		void Clear();
		void Begin(
			std::string label,
			EditorDocumentState before);
		bool Commit(EditorDocumentState after);
		void Cancel();
		bool Push(
			std::string label,
			EditorDocumentState before,
			EditorDocumentState after);
		void RebaseDocument(
			const std::string& documentType,
			const std::string& documentId);

		[[nodiscard]] const EditorDocumentState* Undo();
		[[nodiscard]] const EditorDocumentState* Redo();
		[[nodiscard]] bool CanUndo() const;
		[[nodiscard]] bool CanRedo() const;
		[[nodiscard]] bool HasOpenTransaction() const;
		[[nodiscard]] const char* GetUndoLabel() const;
		[[nodiscard]] const char* GetRedoLabel() const;
		[[nodiscard]] std::size_t GetEntryCount() const;
		[[nodiscard]] std::size_t GetMemoryUsage() const;

	private:
		void TrimToBudget();
		void RemoveRedoEntries();

		std::vector<EditorHistoryEntry> entries_;
		std::size_t cursor_ = 0;
		std::size_t memoryUsage_ = 0;
		std::size_t maximumEntries_ = 128;
		std::size_t maximumBytes_ = 128 * 1024 * 1024;
		bool transactionOpen_ = false;
		std::string transactionLabel_;
		EditorDocumentState transactionBefore_;
	};
}
