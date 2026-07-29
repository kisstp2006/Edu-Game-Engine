#include "../EditorHistory.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
	EGE::EditorDocumentState State(
		std::string payload,
		uint selected = 0)
	tter {
		EGE::EditorDocumentState state;
		state.documentId = "Assets/Scenes/Test.scene";
		state.payload = std::move(payload);
		if (selected != 0)
		{
			state.selectedObjects.push_back(selected);
			state.primaryObject = selected;
		}
		return state;
	}

	bool Expect(bool condition, const char* message)
	{
		if (condition)
			return true;
		std::cerr << message << '\n';
		return false;
	}
}

int main()
{
	EGE::EditorHistory history;
	if (!Expect(
			history.Push("Create", State("A"), State("B", 2)),
			"First edit was not recorded.") ||
		!Expect(history.CanUndo(), "Undo should be available.") ||
		!Expect(
			std::string(history.GetUndoLabel()) == "Create",
			"Undo label is incorrect."))
	{
		return EXIT_FAILURE;
	}

	const EGE::EditorDocumentState* undo = history.Undo();
	if (!Expect(undo && undo->payload == "A", "Undo state is incorrect.") ||
		!Expect(history.CanRedo(), "Redo should be available."))
	{
		return EXIT_FAILURE;
	}

	const EGE::EditorDocumentState* redo = history.Redo();
	if (!Expect(
			redo && redo->payload == "B" &&
				redo->primaryObject == 2,
			"Redo state or selection is incorrect."))
	{
		return EXIT_FAILURE;
	}

	history.RebaseDocument("Scene", "Assets/Scenes/Renamed.scene");
	history.Undo();
	const EGE::EditorDocumentState* renamedRedo = history.Redo();
	if (!Expect(
			renamedRedo &&
				renamedRedo->documentId ==
					"Assets/Scenes/Renamed.scene",
			"History was not rebased after a scene rename."))
	{
		return EXIT_FAILURE;
	}

	history.Begin("Transform", State("B", 2));
	if (!Expect(
			history.Commit(State("C", 2)),
			"Transaction was not committed.") ||
		!Expect(
			history.GetEntryCount() == 2,
			"Transaction should create one history entry."))
	{
		return EXIT_FAILURE;
	}

	history.Undo();
	if (!Expect(
			history.Push("Branch", State("B"), State("D")),
			"Branched edit was not recorded.") ||
		!Expect(
			!history.CanRedo(),
			"A new edit must discard the redo branch."))
	{
		return EXIT_FAILURE;
	}

	history.Begin("No-op", State("D", 4));
	if (!Expect(
			!history.Commit(State("D", 8)),
			"Selection-only changes must not create scene edits."))
	{
		return EXIT_FAILURE;
	}

	EGE::EditorHistory limited(2, 1024 * 1024);
	limited.Push("One", State("0"), State("1"));
	limited.Push("Two", State("1"), State("2"));
	limited.Push("Three", State("2"), State("3"));
	if (!Expect(
			limited.GetEntryCount() == 2,
			"Entry limit was not applied."))
	{
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
