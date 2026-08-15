/*
   Copyright (c) 2026 Christof Ruch. All rights reserved.

   Dual licensed: Distributed under Affero GPL license by default, an MIT license is available for purchase
*/

#include "MainComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>

class SpectroscopeDemoApplication final : public juce::JUCEApplication,
	private juce::Timer {
public:
	const juce::String getApplicationName() override { return "JUCE Spectroscope Demo"; }
	const juce::String getApplicationVersion() override { return "1.0.0"; }
	bool moreThanOneInstanceAllowed() override { return true; }

	void initialise(const juce::String& commandLine) override
	{
		mainWindow_ = std::make_unique<MainWindow>(getApplicationName());
		if (commandLine.contains("--exit-smoke-test"))
			startTimer(1000);
	}

	void shutdown() override
	{
		mainWindow_.reset();
	}

	void systemRequestedQuit() override
	{
		quit();
	}

	void anotherInstanceStarted(const juce::String&) override {}

private:
	void timerCallback() override
	{
		stopTimer();
		systemRequestedQuit();
	}

	class MainWindow final : public juce::DocumentWindow {
	public:
		explicit MainWindow(const juce::String& name)
			: DocumentWindow(name,
				juce::Desktop::getInstance().getDefaultLookAndFeel()
					.findColour(juce::ResizableWindow::backgroundColourId),
				juce::DocumentWindow::allButtons)
		{
			setUsingNativeTitleBar(true);
			setContentOwned(new MainComponent(), true);
			setResizable(true, true);
			centreWithSize(900, 650);
			setVisible(true);
		}

		void closeButtonPressed() override
		{
			juce::JUCEApplication::getInstance()->systemRequestedQuit();
		}

	private:
		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
	};

	std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION(SpectroscopeDemoApplication)
