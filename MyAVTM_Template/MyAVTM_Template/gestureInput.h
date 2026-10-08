#ifndef _gestureInput_
#define _gestureInput_

enum class GestureCmd { None, CamNext, CamPrev, Pause, Resume };

namespace gestureInput
{
	// Abre um socket UDP não bloqueante em 127.0.0.1:port
	bool init(unsigned short port = 5005);

	// Devolve o próximo comando pendente, ou GestureCmd::None se não houver
	GestureCmd poll();

	void shutdown();
}

#endif