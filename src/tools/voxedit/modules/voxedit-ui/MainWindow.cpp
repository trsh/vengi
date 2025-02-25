/**
 * @file
 */

#include "MainWindow.h"
#include "ScopedStyle.h"
#include "Viewport.h"
#include "command/CommandHandler.h"
#include "core/StringUtil.h"
#include "core/collection/DynamicArray.h"
#include "ui/imgui/IconsForkAwesome.h"
#include "ui/imgui/IconsFontAwesome5.h"
#include "ui/imgui/IMGUIApp.h"
#include "ui/imgui/FileDialog.h"
#include "ui/imgui/IMGUIEx.h"
#include "voxedit-util/Config.h"
#include "voxedit-util/SceneManager.h"
#ifdef VOXEDIT_ANIMATION
#include "voxedit-util/anim/AnimationLuaSaver.h"
#endif
#include "voxedit-util/modifier/Modifier.h"
#include "voxelformat/VolumeFormat.h"
#include <glm/gtc/type_ptr.hpp>
#include "ui/imgui/dearimgui/imgui_internal.h"

#define TITLE_STATUSBAR "##statusbar"
#define TITLE_PALETTE "Palette##title"
#define TITLE_POSITIONS "Positions##title"
#define TITLE_ANIMATION_TIMELINE "Animation##animationtimeline"
#define TITLE_TOOLS "Tools##title"
#define TITLE_ASSET "Assets##title"
#define TITLE_LAYERS "Layers##title"
#define TITLE_FORMAT_SETTINGS "Formats##title"
#define TITLE_MODIFIERS "Modifiers##title"
#define TITLE_TREES ICON_FA_TREE " Trees##title"
#define TITLE_SCENEGRAPH "Scenegraph##title"
#define TITLE_SCRIPTPANEL ICON_FA_CODE " Script##title"
#define TITLE_LSYSTEMPANEL ICON_FA_LEAF " L-System##title"
#define TITLE_ANIMATION_SETTINGS "Animation##animationsettings"

#define POPUP_TITLE_UNSAVED "Unsaved Modifications##popuptitle"
#define POPUP_TITLE_NEW_SCENE "New scene##popuptitle"
#define POPUP_TITLE_FAILED_TO_SAVE "Failed to save##popuptitle"
#define POPUP_TITLE_SCENE_SETTINGS "Scene settings##popuptitle"
#define WINDOW_TITLE_SCRIPT_EDITOR ICON_FK_CODE "Script Editor##scripteditor"

namespace voxedit {

MainWindow::MainWindow(ui::imgui::IMGUIApp *app) : _app(app), _assetPanel(app->filesystem()) {
	_scene = new Viewport("free##viewport");
	_sceneTop = new Viewport("top##viewport");
	_sceneLeft = new Viewport("left##viewport");
	_sceneFront = new Viewport("front##viewport");
#ifdef VOXEDIT_ANIMATION
	_sceneAnimation = new Viewport("animation##viewport");
#endif
}

MainWindow::~MainWindow() {
	delete _scene;
	delete _sceneTop;
	delete _sceneLeft;
	delete _sceneFront;
#ifdef VOXEDIT_ANIMATION
	delete _sceneAnimation;
#endif
}

void MainWindow::resetCamera() {
	_scene->resetCamera();
	_sceneTop->resetCamera();
	_sceneLeft->resetCamera();
	_sceneFront->resetCamera();
#ifdef VOXEDIT_ANIMATION
	_sceneAnimation->resetCamera();
#endif
}

void MainWindow::loadLastOpenedFiles(const core::String &string) {
	core::DynamicArray<core::String> tokens;
	core::string::splitString(string, tokens, ";");
	for (const core::String& s : tokens) {
		_lastOpenedFilesRingBuffer.push_back(s);
	}
}

void MainWindow::addLastOpenedFile(const core::String &file) {
	for (const core::String& s : _lastOpenedFilesRingBuffer) {
		if (s == file) {
			return;
		}
	}
	_lastOpenedFilesRingBuffer.push_back(file);
	core::String str;
	for (const core::String& s : _lastOpenedFilesRingBuffer) {
		if (!str.empty()) {
			str.append(";");
		}
		str.append(s);
	}
	_lastOpenedFiles->setVal(str);
}

bool MainWindow::init() {
	_scene->init(voxedit::ViewportController::RenderMode::Editor);
	_scene->controller().setMode(voxedit::ViewportController::SceneCameraMode::Free);

	_sceneTop->init(voxedit::ViewportController::RenderMode::Editor);
	_sceneTop->controller().setMode(voxedit::ViewportController::SceneCameraMode::Top);

	_sceneLeft->init(voxedit::ViewportController::RenderMode::Editor);
	_sceneLeft->controller().setMode(voxedit::ViewportController::SceneCameraMode::Left);

	_sceneFront->init(voxedit::ViewportController::RenderMode::Editor);
	_sceneFront->controller().setMode(voxedit::ViewportController::SceneCameraMode::Front);

#ifdef VOXEDIT_ANIMATION
	_sceneAnimation->init(voxedit::ViewportController::RenderMode::Animation);
	_sceneAnimation->controller().setMode(voxedit::ViewportController::SceneCameraMode::Free);
#endif

	_showGridVar = core::Var::getSafe(cfg::VoxEditShowgrid);
	_modelSpaceVar = core::Var::getSafe(cfg::VoxEditModelSpace);
	_showLockedAxisVar = core::Var::getSafe(cfg::VoxEditShowlockedaxis);
	_showAabbVar = core::Var::getSafe(cfg::VoxEditShowaabb);
	_renderShadowVar = core::Var::getSafe(cfg::VoxEditRendershadow);
	_animationSpeedVar = core::Var::getSafe(cfg::VoxEditAnimationSpeed);
	_gridSizeVar = core::Var::getSafe(cfg::VoxEditGridsize);
	_lastOpenedFile = core::Var::getSafe(cfg::VoxEditLastFile);
	_lastOpenedFiles = core::Var::getSafe(cfg::VoxEditLastFiles);
	loadLastOpenedFiles(_lastOpenedFiles->strVal());

	SceneManager &mgr = sceneMgr();
	voxel::Region region = _layerSettings.region();
	if (!region.isValid()) {
		_layerSettings.reset();
		region = _layerSettings.region();
	}
	if (!mgr.newScene(true, _layerSettings.name, region)) {
		return false;
	}
	afterLoad("");
	return true;
}

void MainWindow::shutdown() {
	_scene->shutdown();
	_sceneTop->shutdown();
	_sceneLeft->shutdown();
	_sceneFront->shutdown();
#ifdef VOXEDIT_ANIMATION
	_sceneAnimation->shutdown();
#endif
}

bool MainWindow::save(const core::String &file) {
	if (!sceneMgr().save(file)) {
		Log::warn("Failed to save the model");
		_popupFailedToSave = true;
		return false;
	}
	Log::info("Saved the model to %s", file.c_str());
	_lastOpenedFile->setVal(file);
	return true;
}

bool MainWindow::load(const core::String &file) {
	if (file.empty()) {
		_app->openDialog([this] (const core::String file) { load(file); }, {}, voxelformat::voxelLoad());
		return true;
	}

	if (!sceneMgr().dirty()) {
		if (sceneMgr().load(file)) {
			afterLoad(file);
			return true;
		}
		return false;
	}

	_loadFile = file;
	_popupUnsaved = true;
	return false;
}

#ifdef VOXEDIT_ANIMATION
bool MainWindow::loadAnimationEntity(const core::String &file) {
	if (!sceneMgr().loadAnimationEntity(file)) {
		return false;
	}
	resetCamera();
	return true;
}
#endif

void MainWindow::afterLoad(const core::String &file) {
	_lastOpenedFile->setVal(file);
	resetCamera();
}

bool MainWindow::createNew(bool force) {
	if (!force && sceneMgr().dirty()) {
		_loadFile.clear();
		_popupUnsaved = true;
	} else {
		_popupNewScene = true;
	}
	return false;
}

bool MainWindow::isLayerWidgetDropTarget() const {
	return _layerPanel.hasFocus();
}

bool MainWindow::isPaletteWidgetDropTarget() const {
	return _palettePanel.hasFocus();
}

void MainWindow::leftWidget() {
	_palettePanel.update(TITLE_PALETTE, _lastExecutedCommand);
	_modifierPanel.update(TITLE_MODIFIERS);
}

void MainWindow::mainWidget() {
	_scene->update();
	_sceneTop->update();
	_sceneLeft->update();
	_sceneFront->update();
#ifdef VOXEDIT_ANIMATION
	_sceneAnimation->update();
#endif
	_animationTimeline.update(TITLE_ANIMATION_TIMELINE, _dockIdMainDown);
}

void MainWindow::rightWidget() {
	_positionsPanel.update(TITLE_POSITIONS, _lastExecutedCommand);
	_toolsPanel.update(TITLE_TOOLS, _lastExecutedCommand);
	_assetPanel.update(TITLE_ASSET, _lastExecutedCommand);
	_animationPanel.update(TITLE_ANIMATION_SETTINGS, _lastExecutedCommand);

	_layerPanel.update(TITLE_LAYERS, &_layerSettings, _lastExecutedCommand);
	_sceneGraphPanel.update(_scene->camera(), TITLE_SCENEGRAPH, _lastExecutedCommand);
	_scriptPanel.update(TITLE_SCRIPTPANEL, WINDOW_TITLE_SCRIPT_EDITOR, _app, _dockIdMainDown);
	_treePanel.update(TITLE_TREES);
	_lsystemPanel.update(TITLE_LSYSTEMPANEL);
}

void MainWindow::updateSettings() {
	SceneManager &mgr = sceneMgr();
	mgr.setGridResolution(_gridSizeVar->intVal());
	mgr.setRenderLockAxis(_showLockedAxisVar->boolVal());
	mgr.setRenderShadow(_renderShadowVar->boolVal());

	render::GridRenderer &gridRenderer = mgr.gridRenderer();
	gridRenderer.setRenderAABB(_showAabbVar->boolVal());
	gridRenderer.setRenderGrid(_showGridVar->boolVal());
}

void MainWindow::dialog(const char *icon, const char *text) {
	ImGui::AlignTextToFramePadding();
	ImGui::PushFont(_app->bigFont());
	ImGui::TextUnformatted(icon);
	ImGui::PopFont();
	ImGui::SameLine();
	ImGui::Spacing();
	ImGui::SameLine();
	ImGui::TextUnformatted(text);
	ImGui::Spacing();
	ImGui::Separator();
}

void MainWindow::registerPopups() {
	if (_popupUnsaved) {
		ImGui::OpenPopup(POPUP_TITLE_UNSAVED);
		_popupUnsaved = false;
	}
	if (_popupNewScene) {
		ImGui::OpenPopup(POPUP_TITLE_NEW_SCENE);
		_popupNewScene = false;
	}
	if (_popupFailedToSave) {
		ImGui::OpenPopup(POPUP_TITLE_FAILED_TO_SAVE);
		_popupFailedToSave = false;
	}
	if (_menuBar._popupSceneSettings) {
		ImGui::OpenPopup(POPUP_TITLE_SCENE_SETTINGS);
		_menuBar._popupSceneSettings = false;
	}

	if (ImGui::BeginPopup(POPUP_TITLE_SCENE_SETTINGS, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted("Scene settings");
		ImGui::Separator();
		glm::vec3 col;

		col = core::Var::getSafe(cfg::VoxEditAmbientColor)->vec3Val();
		if (ImGui::ColorEdit3("Diffuse color", glm::value_ptr(col))) {
			const core::String &c = core::string::format("%f %f %f", col.x, col.y, col.z);
			core::Var::getSafe(cfg::VoxEditAmbientColor)->setVal(c);
		}

		col = core::Var::getSafe(cfg::VoxEditDiffuseColor)->vec3Val();
		if (ImGui::ColorEdit3("Ambient color", glm::value_ptr(col))) {
			const core::String &c = core::string::format("%f %f %f", col.x, col.y, col.z);
			core::Var::getSafe(cfg::VoxEditDiffuseColor)->setVal(c);
		}

#if 0
		glm::vec3 sunPosition = sceneMgr().renderer().shadow().sunPosition();
		if (ImGui::InputVec3("Sun position", sunPosition)) {
			sceneMgr().renderer().setSunPosition(sunPosition, glm::zero<glm::vec3>(), glm::up);
		}
#endif
#if 0
		glm::vec3 sunDirection = sceneMgr().renderer().shadow().sunDirection();
		if (ImGui::InputVec3("Sun direction", sunDirection)) {
			// TODO: sun direction
		}
#endif

		if (ImGui::Button(ICON_FA_CHECK " Done##scenesettings")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal(POPUP_TITLE_UNSAVED, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		dialog(ICON_FA_QUESTION, "There are unsaved modifications.\nDo you wish to discard them?");
		if (ImGui::Button(ICON_FA_CHECK " Yes##unsaved")) {
			ImGui::CloseCurrentPopup();
			if (!_loadFile.empty()) {
				sceneMgr().load(_loadFile);
				afterLoad(_loadFile);
			} else {
				createNew(true);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TIMES " No##unsaved")) {
			ImGui::CloseCurrentPopup();
			_loadFile.clear();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup(POPUP_TITLE_FAILED_TO_SAVE, ImGuiWindowFlags_AlwaysAutoResize)) {
		dialog(ICON_FA_EXCLAMATION_TRIANGLE, "Failed to save the model!");
		if (ImGui::Button(ICON_FA_CHECK " OK##failedsave")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal(POPUP_TITLE_NEW_SCENE, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::InputText("Name", &_layerSettings.name);
		ImGui::InputVec3("Position", _layerSettings.position);
		ImGui::InputVec3("Size", _layerSettings.size);
		if (ImGui::Button(ICON_FA_CHECK " OK##newscene")) {
			ImGui::CloseCurrentPopup();
			const voxel::Region &region = _layerSettings.region();
			if (voxedit::sceneMgr().newScene(true, _layerSettings.name, region)) {
				afterLoad("");
			}
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TIMES " Close##newscene")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void MainWindow::update() {
	core_trace_scoped(MainWindow);
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	const float statusBarHeight = (float)_app->fontSize() + 16.0f;

	if (_lastOpenedFile->isDirty()) {
		_lastOpenedFile->markClean();
		addLastOpenedFile(_lastOpenedFile->strVal());
	}

	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - statusBarHeight));
	ImGui::SetNextWindowViewport(viewport->ID);
	{
		ui::imgui::ScopedStyle style;
		style.setWindowRounding(0.0f);
		style.setWindowBorderSize(0.0f);
		style.setWindowPadding(ImVec2(0.0f, 0.0f));
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoMove;
		if (!core::Var::getSafe(cfg::ClientFullscreen)->boolVal()) {
			windowFlags |= ImGuiWindowFlags_NoTitleBar;
		}
		if (sceneMgr().dirty()) {
			windowFlags |= ImGuiWindowFlags_UnsavedDocument;
		}

		core::String windowTitle = core::string::extractFilenameWithExtension(_lastOpenedFile->strVal());
		if (windowTitle.empty()) {
			windowTitle = _app->appname();
		} else {
			windowTitle.append(" - ");
			windowTitle.append(_app->appname());
		}
		windowTitle.append("###app");
		static bool keepRunning = true;
		if (!ImGui::Begin(windowTitle.c_str(), &keepRunning, windowFlags)) {
			ImGui::SetWindowCollapsed(ImGui::GetCurrentWindow(), false);
			ImGui::End();
			_app->minimize();
			return;
		}
		if (!keepRunning) {
			_app->requestQuit();
		}
	}

	_menuBar.setLastOpenedFiles(_lastOpenedFilesRingBuffer);
	if (_menuBar.update(_app, _lastExecutedCommand)) {
		_initializedDockSpace = false;
	}

	const ImGuiID dockspaceId = ImGui::GetID("DockSpace");
	ImGui::DockSpace(dockspaceId);

	leftWidget();
	mainWidget();
	rightWidget();

	registerPopups();

	ImGui::End();

	_statusBar.update(TITLE_STATUSBAR, statusBarHeight, _lastExecutedCommand.command);

	if (!_initializedDockSpace && viewport->WorkSize.x > 0.0f) {
		ImGui::DockBuilderRemoveNode(dockspaceId);
		ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);
		_dockIdMain = dockspaceId;
		_dockIdLeft = ImGui::DockBuilderSplitNode(_dockIdMain, ImGuiDir_Left, 0.13f, nullptr, &_dockIdMain);
		_dockIdRight = ImGui::DockBuilderSplitNode(_dockIdMain, ImGuiDir_Right, 0.20f, nullptr, &_dockIdMain);
		_dockIdLeftDown = ImGui::DockBuilderSplitNode(_dockIdLeft, ImGuiDir_Down, 0.35f, nullptr, &_dockIdLeft);
		_dockIdRightDown = ImGui::DockBuilderSplitNode(_dockIdRight, ImGuiDir_Down, 0.50f, nullptr, &_dockIdRight);
		_dockIdMainDown = ImGui::DockBuilderSplitNode(_dockIdMain, ImGuiDir_Down, 0.20f, nullptr, &_dockIdMain);
		ImGui::DockBuilderDockWindow(TITLE_PALETTE, _dockIdLeft);
		ImGui::DockBuilderDockWindow(TITLE_POSITIONS, _dockIdRight);
		ImGui::DockBuilderDockWindow(TITLE_ASSET, _dockIdRight);
		ImGui::DockBuilderDockWindow(TITLE_TOOLS, _dockIdRight);
		ImGui::DockBuilderDockWindow(TITLE_ANIMATION_SETTINGS, _dockIdRight);
		ImGui::DockBuilderDockWindow(TITLE_FORMAT_SETTINGS, _dockIdRight);
		ImGui::DockBuilderDockWindow(TITLE_LAYERS, _dockIdRightDown);
		ImGui::DockBuilderDockWindow(TITLE_TREES, _dockIdRightDown);
		ImGui::DockBuilderDockWindow(TITLE_SCENEGRAPH, _dockIdRightDown);
		ImGui::DockBuilderDockWindow(TITLE_LSYSTEMPANEL, _dockIdRightDown);
		ImGui::DockBuilderDockWindow(TITLE_SCRIPTPANEL, _dockIdRightDown);
		ImGui::DockBuilderDockWindow(TITLE_MODIFIERS, _dockIdLeftDown);
		ImGui::DockBuilderDockWindow(_scene->id().c_str(), _dockIdMain);
		ImGui::DockBuilderDockWindow(_sceneLeft->id().c_str(), _dockIdMain);
		ImGui::DockBuilderDockWindow(_sceneTop->id().c_str(), _dockIdMain);
		ImGui::DockBuilderDockWindow(_sceneFront->id().c_str(), _dockIdMain);
#ifdef VOXEDIT_ANIMATION
		ImGui::DockBuilderDockWindow(_sceneAnimation->id().c_str(), _dockIdMain);
#endif
		ImGui::DockBuilderDockWindow(WINDOW_TITLE_SCRIPT_EDITOR, _dockIdMainDown);
		ImGui::DockBuilderDockWindow(TITLE_ANIMATION_TIMELINE, _dockIdMainDown);
		ImGui::DockBuilderFinish(dockspaceId);
		_initializedDockSpace = true;
	}

	updateSettings();
}

bool MainWindow::isSceneHovered() const {
	return _scene->isHovered() || _sceneTop->isHovered() ||
		   _sceneLeft->isHovered() || _sceneFront->isHovered()
#ifdef VOXEDIT_ANIMATION
		    ||
		   _sceneAnimation->isHovered()
#endif
	;
}

bool MainWindow::saveScreenshot(const core::String& file) {
	if (!_scene->saveImage(file.c_str())) {
		Log::warn("Failed to save screenshot to file '%s'", file.c_str());
		return false;
	}
	Log::info("Screenshot created at '%s'", file.c_str());
	return true;
}

} // namespace voxedit
