#include "qt/view/graphElements/QtGraphEdge.h"

#include <QGraphicsSceneEvent>

#include "utility/messaging/type/MessageActivateEdge.h"
#include "utility/messaging/type/MessageFocusIn.h"
#include "utility/messaging/type/MessageFocusOut.h"
#include "utility/messaging/type/MessageGraphNodeBundleSplit.h"
#include "utility/utility.h"

#include "component/view/GraphViewStyle.h"
#include "data/graph/Edge.h"
#include "data/graph/token_component/TokenComponentAggregation.h"
#include "qt/graphics/QtAngledLineItem.h"
#include "qt/graphics/QtStraightLineItem.h"
#include "qt/view/graphElements/QtGraphNode.h"

QtGraphEdge::QtGraphEdge(
	const std::weak_ptr<QtGraphNode>& owner,
	const std::weak_ptr<QtGraphNode>& target,
	const Edge* data,
	size_t weight,
	bool isActive,
	TokenComponentAggregation::Direction direction
)
	: m_data(data)
	, m_owner(owner)
	, m_target(target)
	, m_child(nullptr)
	, m_isActive(isActive)
	, m_fromActive(false)
	, m_toActive(false)
	, m_weight(weight)
	, m_direction(direction)
	, m_mousePos(0.0f, 0.0f)
	, m_mouseMoved(false)
{
	if (m_direction == TokenComponentAggregation::DIRECTION_BACKWARD)
	{
		m_owner.swap(m_target);
	}

	m_fromActive = m_owner.lock()->getIsActive();
	m_toActive = m_target.lock()->getIsActive();

	this->updateLine();
}

QtGraphEdge::~QtGraphEdge()
{
}

const Edge* QtGraphEdge::getData() const
{
	return m_data;
}

std::weak_ptr<QtGraphNode> QtGraphEdge::getOwner()
{
	return m_owner;
}

std::weak_ptr<QtGraphNode> QtGraphEdge::getTarget()
{
	return m_target;
}

void QtGraphEdge::updateLine()
{
	std::shared_ptr<QtGraphNode> owner = m_owner.lock();
	std::shared_ptr<QtGraphNode> target = m_target.lock();

	if (owner == NULL || target == NULL)
	{
		LOG_WARNING("Either the owner or the target node is null.");
		return;
	}

	Edge::EdgeType type;
	if (getData())
	{
		type = getData()->getType();
	}
	else
	{
		type = Edge::EDGE_AGGREGATION;
	}

	GraphViewStyle::EdgeStyle style = GraphViewStyle::getStyleForEdgeType(type, m_isActive, false);

	if (style.isStraight)
	{
		if (!m_child)
		{
			m_child = new QtStraightLineItem(this);
		}

		bool showArrow = m_direction != TokenComponentAggregation::DIRECTION_NONE;

		GraphViewStyle::NodeStyle countStyle = GraphViewStyle::getStyleOfCountCircle();

		dynamic_cast<QtStraightLineItem*>(m_child)->updateLine(
			owner->getBoundingRect(), target->getBoundingRect(), m_weight, style, countStyle, showArrow);
	}
	else
	{
		if (!m_child)
		{
			m_child = new QtAngledLineItem(this);
		}

		QtAngledLineItem* child = dynamic_cast<QtAngledLineItem*>(m_child);

		if (m_fromActive && owner->getLastParent() == target->getLastParent())
		{
			child->setOnBack(true);
		}

		if (m_toActive)
		{
			child->setHorizontalIn(true);

			if (owner->getLastParent() == target->getLastParent())
			{
				child->setOnFront(true);
			}
		}

		if (type != Edge::EDGE_INHERITANCE && type != Edge::EDGE_AGGREGATION)
		{
			child->setRoute(QtAngledLineItem::ROUTE_HORIZONTAL);
		}

		bool showArrow = true;
		if (type == Edge::EDGE_AGGREGATION)
		{
			child->setPivot(QtAngledLineItem::PIVOT_MIDDLE);

			showArrow = m_direction != TokenComponentAggregation::DIRECTION_NONE;
		}

		child->updateLine(
			owner->getBoundingRect(), target->getBoundingRect(),
			owner->getParentBoundingRect(), target->getParentBoundingRect(),
			style, m_weight, showArrow);
	}

	QString toolTip = Edge::getTypeString(type).c_str();
	if (type == Edge::EDGE_AGGREGATION)
	{
		toolTip += ": " + QString::number(m_weight) + " edge";
		if (m_weight != 1)
		{
			toolTip += "s";
		}
	}
	m_child->setToolTip(toolTip);

	this->setZValue(style.zValue); // Used to draw edges always on top of nodes.
}

bool QtGraphEdge::getIsActive() const
{
	return m_isActive;
}

void QtGraphEdge::setIsActive(bool isActive)
{
	if (m_isActive != isActive)
	{
		m_isActive = isActive;
		updateLine();
	}
}

void QtGraphEdge::onClick()
{
	if (!getData())
	{
		std::weak_ptr<QtGraphNode> node =
			(m_direction == TokenComponentAggregation::DIRECTION_BACKWARD ? m_owner : m_target);
		MessageGraphNodeBundleSplit(node.lock()->getTokenId()).dispatch();
	}
	else
	{
		MessageActivateEdge msg(
			getData()->getId(),
			getData()->getType(),
			getData()->getFrom()->getNameHierarchy(),
			getData()->getTo()->getNameHierarchy()
		);

		if (getData()->getType() == Edge::EDGE_AGGREGATION)
		{
			msg.aggregationIds =
				utility::toVector<Id>(getData()->getComponent<TokenComponentAggregation>()->getAggregationIds());
		}

		msg.dispatch();
	}
}

void QtGraphEdge::focusIn()
{
	bool isActive = m_isActive;
	this->setIsActive(true);
	m_isActive = isActive;
}

void QtGraphEdge::focusOut()
{
	updateLine();
}

void QtGraphEdge::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	m_mousePos = Vec2i(event->scenePos().x(), event->scenePos().y());
	m_mouseMoved = false;
}

void QtGraphEdge::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	Vec2i mousePos = Vec2i(event->scenePos().x(), event->scenePos().y());

	if ((mousePos - m_mousePos).getLength() > 1.0f)
	{
		m_mouseMoved = true;
	}
}

void QtGraphEdge::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if (!m_mouseMoved)
	{
		this->onClick();
	}
}

void QtGraphEdge::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
	if (!getData())
	{
		focusIn();
		return;
	}

	MessageFocusIn(std::vector<Id>(1, getData()->getId())).dispatch();
}

void QtGraphEdge::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
	if (!getData())
	{
		focusOut();
		return;
	}

	MessageFocusOut(std::vector<Id>(1, getData()->getId())).dispatch();
}

void QtGraphEdge::setDirection(TokenComponentAggregation::Direction direction)
{
	if (m_direction != direction)
	{
		m_direction = direction;
		updateLine();
	}
}
