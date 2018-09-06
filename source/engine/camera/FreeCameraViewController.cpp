#include "FreeCameraViewController.h"
#include "../util/Timer.h"
#include "../input/Input.h"
#include "CameraComponent.h"

FreeCameraViewController::FreeCameraViewController()
{
}

void FreeCameraViewController::update()
{
	auto camera = getComponent<CameraComponent>();
	if (!camera)
		return;

	bool moved = false;
	float speed = movementSpeed * Time::deltaTime();

	if (Input::isKeyDown(SDL_SCANCODE_W))
	{
		camera->walk(speed);
		moved = true;
	}

	if (Input::isKeyDown(SDL_SCANCODE_S))
	{
		camera->walk(-speed);
		moved = true;
	}

	if (Input::isKeyDown(SDL_SCANCODE_A))
	{
		camera->strafe(-speed);
		moved = true;
	}

	if (Input::isKeyDown(SDL_SCANCODE_D))
	{
		camera->strafe(speed);
		moved = true;
	}

	if (Input::rightDrag().isDragging())
	{
		float dx = -Input::rightDrag().getDragDelta().x;
		float dy = Input::rightDrag().getDragDelta().y;

		camera->pitch(math::toRadians(dy * 0.1f));
		camera->rotateY(math::toRadians(dx * 0.1f));
		SDL_SetRelativeMouseMode(SDL_TRUE);
		moved = true;
	}
	else
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}

	bMovedInLastUpdate = moved;
}

std::string FreeCameraViewController::getName() const
{
	return "FreeCameraViewController";
}
