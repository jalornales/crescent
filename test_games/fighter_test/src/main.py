from test_games.fighter_test.src.fight_sim.fight_sim import *
from test_games.fighter_test.src.game_state import *
from test_games.fighter_test.src.timer import Timer

fighting_round_timer = Timer(99.0)


class Main(Node2D):
    def _start(self) -> None:
        self.game_state = GameState()

        Engine.set_fps_display_enabled(True)

        # Fighter Data
        self.time_display = self.get_child("TimeDisplay")
        # Nodes
        if self.game_state.mode == GameMode.ONLINE_PVP_CLIENT:
            player_one_node = self.get_child(name="PlayerTwo")
            player_two_node = self.get_child(name="PlayerOne")
        else:
            player_one_node = self.get_child(name="PlayerOne")
            player_two_node = self.get_child(name="PlayerTwo")
        player_one_collider = player_one_node.get_child(name="Collider")
        player_two_collider = player_two_node.get_child(name="Collider")
        print(f"[PYTHON_SCRIPT] p1 = {player_one_node}, p2 = {player_two_node}")

        player_one_node.play(animation_name="crouch")

        # Test Get Parent
        # parent = player_one_node.get_parent()
        # print(f"[PY_SCRIPT] parent = {parent}")

        # Input Buffers
        player_one_input, player_two_input = self._get_input_buffers_from_game_mode(
            self.game_state.mode
        )
        # Fight Sim
        self.fight_sim = FighterSimulation(self)
        # Spawn Health Bars
        p1_health_bar = HealthBar.new()
        p1_health_bar.position = Vector2(100, 80)
        self.add_child(p1_health_bar)
        p2_health_bar = HealthBar.new()
        p2_health_bar.position = Vector2(600, 80)
        self.add_child(p2_health_bar)
        # Add fighters
        self.fight_sim.add_fighter(
            Fighter(
                player_one_node, player_one_collider, player_one_input, p1_health_bar
            )
        )
        self.fight_sim.add_fighter(
            Fighter(
                player_two_node, player_two_collider, player_two_input, p2_health_bar
            )
        )

        # self.fight_sim.add_health_bars()

        # Network
        is_network_enabled = (
            self.game_state.mode == GameMode.ONLINE_PVP_HOST
            or self.game_state.mode == GameMode.ONLINE_PVP_CLIENT
        )
        if is_network_enabled:
            if self.game_state.mode == GameMode.ONLINE_PVP_HOST:
                Server.subscribe(
                    signal_id="poll",
                    listener_node=self,
                    listener_func=self._network_server_callback,
                )
                print("[PYTHON SCRIPT] Server")
            else:
                Client.subscribe(
                    signal_id="poll",
                    listener_node=self,
                    listener_func=self._network_client_callback,
                )
                print("[PYTHON SCRIPT] Client")

    def _update(self, delta_time: float) -> None:
        if Input.is_action_just_pressed(name="exit"):
            Engine.exit()

        if Input.is_action_just_pressed(name="ui_confirm"):
            SceneTree.change_scene(path="test_games/fighter_test/nodes/main_node.py")

        if Input.is_action_just_pressed(name="play_sfx"):
            self.fight_sim.fighters[0].health_bar.print_debug_info()
            self.fight_sim.fighters[0].health_bar.set_health_percentage(50)
            self.fight_sim.fighters[0].health_bar.print_debug_info()

    def _physics_update(self, delta_time: float) -> None:
        # Temp camera test
        camera_speed = Vector2(5.0, 5.0)
        if Input.is_action_pressed("camera_left"):
            Camera2D.add_to_position(Vector2.LEFT() * camera_speed)
        if Input.is_action_pressed("camera_right"):
            Camera2D.add_to_position(Vector2.RIGHT() * camera_speed)
        if Input.is_action_pressed("camera_up"):
            Camera2D.add_to_position(Vector2.UP() * camera_speed)
        if Input.is_action_pressed("camera_down"):
            Camera2D.add_to_position(Vector2.DOWN() * camera_speed)

        if Input.is_action_pressed("rotate_pos"):
            self.fight_sim.fighters[0].node.add_to_rotation(1)
        if Input.is_action_pressed("rotate_neg"):
            self.fight_sim.fighters[0].node.add_to_rotation(-1)

        self.fight_sim.update(delta_time)

        self.time_display.text = f"{int(fighting_round_timer.tick(delta_time))}"

    def _network_server_callback(self, message: str) -> None:
        # print(
        #     f"[PYTHON SCRIPT] [SERVER] _network_server_callback - message: '{message}'"
        # )
        self.fight_sim.network_update(message=message)

    def _network_client_callback(self, message: str) -> None:
        # print(
        #     f"[PYTHON SCRIPT] [CLIENT] _network_client_callback - message: '{message}'"
        # )
        self.fight_sim.network_update(message=message)

    def _get_input_buffers_from_game_mode(self, game_mode: str) -> tuple:
        player_one_input = None
        player_two_input = None
        if game_mode == GameMode.LOCAL_AI:
            player_one_input = InputBuffer(
                move_left_action_name="p1_move_left",
                move_right_action_name="p1_move_right",
                crouch_action_name="p1_crouch",
                jump_action_name="p1_jump",
                light_punch_action_name="p1_light_punch",
            )
            player_two_input = AIInputBuffer(
                move_left_action_name="p2_move_left",
                move_right_action_name="p2_move_right",
                crouch_action_name="p2_crouch",
                jump_action_name="p2_jump",
                light_punch_action_name="p2_light_punch",
            )
        elif game_mode == GameMode.LOCAL_PVP:
            player_one_input = InputBuffer(
                move_left_action_name="p1_move_left",
                move_right_action_name="p1_move_right",
                crouch_action_name="p1_crouch",
                jump_action_name="p1_jump",
                light_punch_action_name="p1_light_punch",
            )
            player_two_input = InputBuffer(
                move_left_action_name="p2_move_left",
                move_right_action_name="p2_move_right",
                crouch_action_name="p2_crouch",
                jump_action_name="p2_jump",
                light_punch_action_name="p2_light_punch",
            )
        elif (
            game_mode == GameMode.ONLINE_PVP_HOST
            or game_mode == GameMode.ONLINE_PVP_CLIENT
        ):
            player_one_input = NetworkSenderInputBuffer(
                move_left_action_name="p1_move_left",
                move_right_action_name="p1_move_right",
                crouch_action_name="p1_crouch",
                jump_action_name="p1_jump",
                light_punch_action_name="p1_light_punch",
            )
            if game_mode == GameMode.ONLINE_PVP_CLIENT:
                player_one_input.send_func = Client.send
            player_two_input = NetworkReceiverInputBuffer(
                move_left_action_name="p2_move_left",
                move_right_action_name="p2_move_right",
                crouch_action_name="p2_crouch",
                jump_action_name="p2_jump",
                light_punch_action_name="p2_light_punch",
            )
        elif game_mode == GameMode.ONLINE_PVP_HOST:
            player_one_input = NetworkSenderInputBuffer(
                move_left_action_name="p1_move_left",
                move_right_action_name="p1_move_right",
                crouch_action_name="p1_crouch",
                jump_action_name="p1_jump",
                light_punch_action_name="p1_light_punch",
            )
            player_two_input = NetworkReceiverInputBuffer(
                move_left_action_name="p2_move_left",
                move_right_action_name="p2_move_right",
                crouch_action_name="p2_crouch",
                jump_action_name="p2_jump",
                light_punch_action_name="p2_light_punch",
            )
        elif game_mode == GameMode.ONLINE_PVP_CLIENT:
            player_two_input = NetworkSenderInputBuffer(
                move_left_action_name="p1_move_left",
                move_right_action_name="p1_move_right",
                crouch_action_name="p1_crouch",
                jump_action_name="p1_jump",
                light_punch_action_name="p1_light_punch",
            )
            player_two_input.send_func = Client.send
            player_one_input = NetworkReceiverInputBuffer(
                move_left_action_name="p2_move_left",
                move_right_action_name="p2_move_right",
                crouch_action_name="p2_crouch",
                jump_action_name="p2_jump",
                light_punch_action_name="p2_light_punch",
            )
        return player_one_input, player_two_input
