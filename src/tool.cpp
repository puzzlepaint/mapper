/*
 *    Copyright 2012, 2013 Thomas Schöps
 *
 *    This file is part of OpenOrienteering.
 *
 *    OpenOrienteering is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    OpenOrienteering is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with OpenOrienteering.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "tool.h"

#include <QApplication>
#include <QPainter>
#include <QMouseEvent>
#include <QTimer>

#include "gui/main_window.h"
#include "map.h"
#include "map_editor.h"
#include "map_undo.h"
#include "map_widget.h"
#include "object.h"
#include "object_text.h"
#include "settings.h"
#include "tool_helpers.h"
#include "util.h"

// ### MapEditorTool ###

const QRgb MapEditorTool::inactive_color = qRgb(0, 0, 255);
const QRgb MapEditorTool::active_color = qRgb(255, 150, 0);
const QRgb MapEditorTool::selection_color = qRgb(210, 0, 229);
QImage* MapEditorTool::point_handles = NULL;

MapEditorTool::MapEditorTool(MapEditorController* editor, Type type, QAction* tool_button)
: QObject(NULL),
  tool_button(tool_button),
  type(type),
  uses_touch_cursor(true),
  editor(editor)
{
}

MapEditorTool::~MapEditorTool()
{
	if (tool_button)
		tool_button->setChecked(false);
}

void MapEditorTool::init()
{
	// nothing
}

void MapEditorTool::deactivate()
{
	if (getType() == MapEditorTool::EditPoint)
		editor->setTool(NULL);
	else
		editor->setEditTool();
}

void MapEditorTool::switchToDefaultDrawTool(Symbol* symbol) const
{
	editor->setTool(editor->getDefaultDrawToolForSymbol(symbol));
}

void MapEditorTool::setEditingInProgress(bool state)
{
	editor->setEditingInProgress(state);
}


void MapEditorTool::draw(QPainter* painter, MapWidget* widget)
{
	Q_UNUSED(painter);
	Q_UNUSED(widget);
	// nothing
}


bool MapEditorTool::mousePressEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	Q_UNUSED(event);
	Q_UNUSED(map_coord);
	Q_UNUSED(widget);
	return false;
}

bool MapEditorTool::mouseMoveEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	Q_UNUSED(event);
	Q_UNUSED(map_coord);
	Q_UNUSED(widget);
	return false;
}

bool MapEditorTool::mouseReleaseEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	Q_UNUSED(event);
	Q_UNUSED(map_coord);
	Q_UNUSED(widget);
	return false;
}

bool MapEditorTool::mouseDoubleClickEvent(QMouseEvent* event, MapCoordF map_coord, MapWidget* widget)
{
	Q_UNUSED(event);
	Q_UNUSED(map_coord);
	Q_UNUSED(widget);
	return false;
}

void MapEditorTool::leaveEvent(QEvent* event)
{
	Q_UNUSED(event);
	// nothing
}


bool MapEditorTool::keyPressEvent(QKeyEvent* event)
{
	Q_UNUSED(event);
	return false;
}

bool MapEditorTool::keyReleaseEvent(QKeyEvent* event)
{
	Q_UNUSED(event);
	return false;
}

void MapEditorTool::focusOutEvent(QFocusEvent* event)
{
	Q_UNUSED(event);
	// nothing
}


Map* MapEditorTool::map() const
{
	return editor->getMap();
}

MapWidget* MapEditorTool::mapWidget() const
{
	return editor->getMainWidget();
}

MainWindow* MapEditorTool::mainWindow() const
{
	return editor->getWindow();
}

QWidget* MapEditorTool::window() const
{
	return editor->getWindow();
}

bool MapEditorTool::isDrawTool(MapEditorTool::Type type)
{
	return type == DrawPoint || type == DrawPath || type == DrawCircle || type == DrawRectangle || type == DrawText;
}

void MapEditorTool::loadPointHandles()
{
	if (!point_handles)
		point_handles = new QImage(":/images/point-handles.png");
}

QRgb MapEditorTool::getPointHandleStateColor(PointHandleState state)
{
	if (state == NormalHandleState)
		return qRgb(0, 0, 255);
	else if (state == ActiveHandleState)
		return qRgb(255, 150, 0);
	else if (state == SelectedHandleState)
		return qRgb(255, 0, 0);
	else //if (state == DisabledHandleState)
		return qRgb(106, 106, 106);
}

void MapEditorTool::setStatusBarText(const QString& text)
{
	editor->getWindow()->setStatusBarText(text);
}

void MapEditorTool::startEditingSelection(MapRenderables& old_renderables, std::vector< Object* >* undo_duplicates)
{
	Map* map = editor->getMap();
	
	// Temporarily take the edited objects out of the map so their map renderables are not updated, and make duplicates of them before for the edit step
	Map::ObjectSelection::const_iterator it_end = map->selectedObjectsEnd();
	for (Map::ObjectSelection::const_iterator it = map->selectedObjectsBegin(); it != it_end; ++it)
	{
		if (undo_duplicates)
			undo_duplicates->push_back((*it)->duplicate());
		
		(*it)->setMap(NULL);
		
		// Cache old renderables until the object is inserted into the map again
		old_renderables.insertRenderablesOfObject(*it);
 		(*it)->takeRenderables();
	}
	
	editor->setEditingInProgress(true);
}

void MapEditorTool::resetEditedObjects(std::vector< Object* >* undo_duplicates)
{
	assert(undo_duplicates);
	Map* map = editor->getMap();
	
	size_t i = 0;
	Map::ObjectSelection::const_iterator it_end = map->selectedObjectsEnd();
	for (Map::ObjectSelection::const_iterator it = map->selectedObjectsBegin(); it != it_end; ++it)
	{
		*(*it) = *undo_duplicates->at(i);
		(*it)->setMap(NULL);
		++i;
	}
}

void MapEditorTool::finishEditingSelection(MapRenderables& renderables, MapRenderables& old_renderables, bool create_undo_step, std::vector< Object* >* undo_duplicates, bool delete_objects)
{
	ReplaceObjectsUndoStep* undo_step = create_undo_step ? new ReplaceObjectsUndoStep(editor->getMap()) : NULL;
	
	int i = 0;
	Map::ObjectSelection::const_iterator it_end = editor->getMap()->selectedObjectsEnd();
	for (Map::ObjectSelection::const_iterator it = editor->getMap()->selectedObjectsBegin(); it != it_end; ++it)
	{
		if (!delete_objects)
		{
			(*it)->setMap(editor->getMap());
			(*it)->update(true);
		}
		
		if (create_undo_step)
			undo_step->addObject(*it, undo_duplicates->at(i));
		else
			delete undo_duplicates->at(i);
		++i;
	}
	renderables.clear();
	deleteOldSelectionRenderables(old_renderables, true);
	
	undo_duplicates->clear();
	if (create_undo_step)
		editor->getMap()->objectUndoManager().addNewUndoStep(undo_step);
	
	editor->setEditingInProgress(false);
}

void MapEditorTool::updateSelectionEditPreview(MapRenderables& renderables)
{
	Map::ObjectSelection::const_iterator it_end = editor->getMap()->selectedObjectsEnd();
	for (Map::ObjectSelection::const_iterator it = editor->getMap()->selectedObjectsBegin(); it != it_end; ++it)
	{
		(*it)->update(true);
		// NOTE: only necessary because of setMap(NULL) in startEditingSelection(..)
		renderables.insertRenderablesOfObject(*it);
	}
}

void MapEditorTool::deleteOldSelectionRenderables(MapRenderables& old_renderables, bool set_area_dirty)
{
	old_renderables.clear(set_area_dirty);
}

void MapEditorTool::includeControlPointRect(QRectF& rect, Object* object)
{
	if (object->getType() == Object::Path)
	{
		PathObject* path = reinterpret_cast<PathObject*>(object);
		int size = path->getCoordinateCount();
		for (int i = 0; i < size; ++i)
			rectInclude(rect, path->getCoordinate(i).toQPointF());
	}
	else if (object->getType() == Object::Text)
	{
		TextObject* text_object = reinterpret_cast<TextObject*>(object);
		if (text_object->hasSingleAnchor())
			rectInclude(rect, text_object->getAnchorCoordF());
		else
		{
			QPointF box_text_handles[4];
			calculateBoxTextHandles(box_text_handles, text_object);
			for (int i = 0; i < 4; ++i)
				rectInclude(rect, box_text_handles[i]);
		}
	}
}

void MapEditorTool::drawPointHandles(int hover_point, QPainter* painter, Object* object, MapWidget* widget, bool draw_curve_handles, PointHandleState base_state)
{
	if (object->getType() == Object::Point)
	{
		PointObject* point = reinterpret_cast<PointObject*>(object);
		drawPointHandle(painter, widget->mapToViewport(point->getCoordF()), NormalHandle, (hover_point == 0) ? ActiveHandleState : base_state);
	}
	else if (object->getType() == Object::Text)
	{
		TextObject* text = reinterpret_cast<TextObject*>(object);
		if (text->hasSingleAnchor())
			drawPointHandle(painter, widget->mapToViewport(text->getAnchorCoordF()), NormalHandle, (hover_point == 0) ? ActiveHandleState : base_state);
		else
		{
			QPointF box_text_handles[4];
			calculateBoxTextHandles(box_text_handles, text);
			for (int i = 0; i < 4; ++i)
				drawPointHandle(painter, widget->mapToViewport(box_text_handles[i]), NormalHandle, (hover_point == i) ? ActiveHandleState : base_state);
		}
	}
	else if (object->getType() == Object::Path)
	{
		painter->setBrush(Qt::NoBrush); // for handle lines
		
		PathObject* path = reinterpret_cast<PathObject*>(object);
		
		int num_parts = path->getNumParts();
		for (int part_index = 0; part_index < num_parts; ++part_index)
		{
			PathObject::PathPart& part = path->getPart(part_index);
			bool have_curve = part.isClosed() && part.getNumCoords() > 3 && path->getCoordinate(part.end_index - 3).isCurveStart();
			PointHandleType handle_type = NormalHandle;
			
			for (int i = part.start_index; i <= part.end_index; ++i)
			{
				MapCoord coord = path->getCoordinate(i);
				if (coord.isClosePoint())
					continue;
				QPointF point = widget->mapToViewport(coord);
				bool is_active = hover_point == i;
				
				if (i == part.start_index && !part.isClosed()) // || (i > part.start_index && path->getCoordinate(i-1).isHolePoint()))
					handle_type = StartHandle;
				else if (i == part.end_index && !part.isClosed()) // || coord.isHolePoint())
					handle_type = EndHandle;
				else
					handle_type = coord.isDashPoint() ? DashHandle : NormalHandle;
				
				// Draw incoming curve handle
				QPointF curve_handle;
				if (draw_curve_handles && have_curve)
				{
					int curve_index = (i == part.start_index) ? (part.end_index - 1) : (i - 1);
					curve_handle = widget->mapToViewport(path->getCoordinate(curve_index));
					drawCurveHandleLine(painter, point, handle_type, curve_handle, is_active ? ActiveHandleState : base_state);
					drawPointHandle(painter, curve_handle, CurveHandle, (is_active || hover_point == curve_index) ? ActiveHandleState : base_state);
					have_curve = false;
				}
				
				// Draw outgoing curve handle, first part
				if (draw_curve_handles && coord.isCurveStart())
				{
					curve_handle = widget->mapToViewport(path->getCoordinate(i+1));
					drawCurveHandleLine(painter, point, handle_type, curve_handle, is_active ? ActiveHandleState : base_state);
				}
				
				// Draw point
				drawPointHandle(painter, point, handle_type, is_active ? ActiveHandleState : base_state);
				
				// Draw outgoing curve handle, second part
				if (coord.isCurveStart())
				{
					if (draw_curve_handles)
					{
						drawPointHandle(painter, curve_handle, CurveHandle, (is_active || hover_point == i + 1) ? ActiveHandleState : base_state);
						have_curve = true;
					}
					i += 2;
				}
			}
		}
	}
	else
		assert(false);
}

void MapEditorTool::drawPointHandle(QPainter* painter, QPointF point, PointHandleType type, PointHandleState state)
{
	painter->drawImage(qRound(point.x()) - 5, qRound(point.y()) - 5, *point_handles, (int)type * 11, (int)state * 11, 11, 11);
}

void MapEditorTool::drawCurveHandleLine(QPainter* painter, QPointF point, PointHandleType type, QPointF curve_handle, PointHandleState state)
{
	const float handle_radius = 3;
	painter->setPen(getPointHandleStateColor(state));
	
	QPointF to_handle = curve_handle - point;
	float to_handle_len = to_handle.x()*to_handle.x() + to_handle.y()*to_handle.y();
	if (to_handle_len > 0.00001f)
	{
		to_handle_len = sqrt(to_handle_len);
		if (type == StartHandle)
			point += 5 / qMax(qAbs(to_handle.x()), qAbs(to_handle.y())) * to_handle;
		else if (type == DashHandle)
			point += to_handle * (3 / to_handle_len);
		else //if (type == NormalHandle)
			point += 3 / qMax(qAbs(to_handle.x()), qAbs(to_handle.y())) * to_handle;
		
		curve_handle -= to_handle * (handle_radius / to_handle_len);
	}
	
	painter->drawLine(point, curve_handle);
}
void MapEditorTool::calculateBoxTextHandles(QPointF* out, TextObject* text_object)
{
	assert(!text_object->hasSingleAnchor());

	QTransform transform;
	transform.rotate(-text_object->getRotation() * 180 / M_PI);
	out[0] = transform.map(QPointF(text_object->getBoxWidth() / 2, -text_object->getBoxHeight() / 2)) + text_object->getAnchorCoordF().toQPointF();
	out[1] = transform.map(QPointF(text_object->getBoxWidth() / 2, text_object->getBoxHeight() / 2)) + text_object->getAnchorCoordF().toQPointF();
	out[2] = transform.map(QPointF(-text_object->getBoxWidth() / 2, text_object->getBoxHeight() / 2)) + text_object->getAnchorCoordF().toQPointF();
	out[3] = transform.map(QPointF(-text_object->getBoxWidth() / 2, -text_object->getBoxHeight() / 2)) + text_object->getAnchorCoordF().toQPointF();
}

int MapEditorTool::findHoverPoint(QPointF cursor, Object* object, bool include_curve_handles, QRectF* selection_extent, MapWidget* widget, MapCoordF* out_handle_pos)
{
	Q_UNUSED(selection_extent);
	
	int click_tolerance = Settings::getInstance().getSettingCached(Settings::MapEditor_ClickTolerance).toInt();
	const float click_tolerance_squared = click_tolerance * click_tolerance;
	
	if (object->getType() == Object::Point)
	{
		PointObject* point = reinterpret_cast<PointObject*>(object);
		if (distanceSquared(widget->mapToViewport(point->getCoordF()), cursor) <= click_tolerance_squared)
		{
			if (out_handle_pos)
				*out_handle_pos = point->getCoordF();
			return 0;
		}
	}
	else if (object->getType() == Object::Text)
	{
		TextObject* text = reinterpret_cast<TextObject*>(object);
		if (text->hasSingleAnchor() && distanceSquared(widget->mapToViewport(text->getAnchorCoordF()), cursor) <= click_tolerance_squared)
		{
			if (out_handle_pos)
				*out_handle_pos = text->getAnchorCoordF();
			return 0;
		}
		else if (!text->hasSingleAnchor())
		{
			QPointF box_text_handles[4];
			calculateBoxTextHandles(box_text_handles, text);
			for (int i = 0; i < 4; ++i)
			{
				if (distanceSquared(widget->mapToViewport(box_text_handles[i]), cursor) <= click_tolerance_squared)
				{
					if (out_handle_pos)
						*out_handle_pos = MapCoordF(box_text_handles[i]);
					return i;
				}
			}
		}
	}
	else if (object->getType() == Object::Path)
	{
		PathObject* path = reinterpret_cast<PathObject*>(object);
		int size = path->getCoordinateCount();
		
		int best_index = -1;
		float best_dist_sq = click_tolerance_squared;
		for (int i = size - 1; i >= 0; --i)
		{
			if (!path->getCoordinate(i).isClosePoint())
			{
				float distance_sq = distanceSquared(widget->mapToViewport(path->getCoordinate(i)), cursor);
				bool is_handle = (i >= 1 && path->getCoordinate(i - 1).isCurveStart()) ||
									(i >= 2 && path->getCoordinate(i - 2).isCurveStart());
				if (distance_sq < best_dist_sq || (distance_sq == best_dist_sq && is_handle))
				{
					best_index = i;
					best_dist_sq = distance_sq;
				}
			}
			if (!include_curve_handles && i >= 3 && path->getCoordinate(i - 3).isCurveStart())
				i -= 2;
		}
		if (best_index >= 0)
		{
			if (out_handle_pos)
				*out_handle_pos = MapCoordF(path->getCoordinate(best_index));
			return best_index;
		}
	}
	
	return -1;
}

bool MapEditorTool::drawMouseButtonHeld(QMouseEvent* event)
{
	if ((event->buttons() & Qt::LeftButton) ||
		(Settings::getInstance().getSettingCached(Settings::MapEditor_DrawLastPointOnRightClick).toBool() == true &&
		 event->buttons() & Qt::RightButton))
	{
		return true;
	}
	return false;
}

bool MapEditorTool::drawMouseButtonClicked(QMouseEvent* event)
{
	if ((event->button() == Qt::LeftButton) ||
		(Settings::getInstance().getSettingCached(Settings::MapEditor_DrawLastPointOnRightClick).toBool() == true &&
		 event->button() == Qt::RightButton))
	{
		return true;
	}
	return false;
}