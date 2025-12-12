import threading
import uvicorn
import asyncio
import logging
import time # For time.sleep in start()
from typing import Optional

# Configure a daemon-specific logger (ensure this is consistent with the daemon's logger)
logger = logging.getLogger("DeviceManagerDaemon")

class DaemonRunner:
    """
    A utility class to run a FastAPI application in a separate thread,
    allowing it to be launched and managed programmatically from another Python component.
    """
    def __init__(self, app, host: str = "0.0.0.0", port: int = 8777):
        self.app = app
        self.host = host
        self.port = port
        self.server_thread: Optional[threading.Thread] = None
        self.config: Optional[uvicorn.Config] = None
        self.server: Optional[uvicorn.Server] = None

    def _run_server(self):
        """Internal method to run the Uvicorn server. This method is blocking."""

        # Create a new event loop for this thread.
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)

        # Configure the Uvicorn server.
        self.config = uvicorn.Config(self.app, host=self.host, port=self.port, log_level="info")
        self.server = uvicorn.Server(self.config)

        # Instead of self.server.run(), we explicitly run the 'serve' coroutine
        # in the loop we just created. This avoids the problematic asyncio.run().
        loop.run_until_complete(self.server.serve())

    def start(self):
        """
        Starts the FastAPI application in a new daemon thread.
        This method returns immediately, allowing the calling component to continue.
        """
        if self.server_thread and self.server_thread.is_alive():
            logger.info("DeviceManagerDaemon is already running.")
            return

        logger.info(f"Starting DeviceManagerDaemon on {self.host}:{self.port} in a separate thread...")
        self.server_thread = threading.Thread(target=self._run_server, daemon=True)
        self.server_thread.start()
        logger.info("DeviceManagerDaemon thread started.")

        # Give the server a moment to initialize. In a real application,
        # you might want a more robust readiness check (e.g., polling an endpoint).
        time.sleep(1)

    def stop(self, timeout: float = 5.0):
        """
        Stops the FastAPI application gracefully.
        Args:
            timeout: The maximum time (in seconds) to wait for the server thread to terminate.
        """
        if self.server and self.server_thread and self.server_thread.is_alive():
            logger.info("Stopping DeviceManagerDaemon...")
            # Signal the Uvicorn server to shut down
            self.server.should_exit = True
            self.server_thread.join(timeout=timeout) # Wait for the thread to finish
            if self.server_thread.is_alive():
                logger.warning("DeviceManagerDaemon thread did not terminate gracefully within timeout.")
            else:
                logger.info("DeviceManagerDaemon stopped.")
            self.server_thread = None
            self.server = None
            self.config = None
        else:
            logger.info("DeviceManagerDaemon is not running.")
