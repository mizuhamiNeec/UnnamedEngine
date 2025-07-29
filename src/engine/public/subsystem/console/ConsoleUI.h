#pragma once

namespace Unnamed {
	class ConsoleSystem;

	class ConsoleUI {
	public:
		explicit ConsoleUI(ConsoleSystem* consoleSystem);

		void Show();

	private:
		ConsoleSystem* mConsoleSystem;
	};
}
