"""
Python wrapper for the s1 C++ module
This file provides a Python interface to the complete C++ implementation of the Student Agent.
"""

import build.student_agent_module as s1
from abc import ABC, abstractmethod
from typing import List, Dict, Any, Optional


def get_opponent(player: str) -> str:
    """Get the opponent player identifier."""
    return "square" if player == "circle" else "circle"


def in_bounds(x: int, y: int, rows: int, cols: int) -> bool:
    """Check if coordinates are within board boundaries."""
    return 0 <= x < cols and 0 <= y < rows


def score_cols_for(cols: int) -> List[int]:
    """Get the column indices for scoring areas."""
    w = 4
    start = max(0, (cols - w) // 2)
    return list(range(start, start + w))


def top_score_row() -> int:
    """Get the row index for Circle's scoring area."""
    return 2


def bottom_score_row(rows: int) -> int:
    """Get the row index for Square's scoring area."""
    return rows - 3


def is_opponent_score_cell(x: int, y: int, player: str, rows: int, cols: int, score_cols: List[int]) -> bool:
    """Check if a cell is in the opponent's scoring area."""
    if player == "circle":
        return (y == bottom_score_row(rows)) and (x in score_cols)
    else:
        return (y == top_score_row()) and (x in score_cols)


def is_own_score_cell(x: int, y: int, player: str, rows: int, cols: int, score_cols: List[int]) -> bool:
    """Check if a cell is in the player's own scoring area."""
    if player == "circle":
        return (y == top_score_row()) and (x in score_cols)
    else:
        return (y == bottom_score_row(rows)) and (x in score_cols)


class BaseAgent(ABC):
    """
    Abstract base class for all agents.
    """
    
    def __init__(self, player: str):
        """Initialize agent with player identifier."""
        self.player = player
        self.opponent = get_opponent(player)
    
    @abstractmethod
    def choose(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int], current_player_time: float, opponent_time: float) -> Optional[Dict[str, Any]]:
        """
        Choose the best move for the current board state.
        
        Args:
            board: 2D list representing the game board
            rows, cols: Board dimensions
            score_cols: List of column indices for scoring areas
        
        Returns:
            Dictionary representing the chosen move, or None if no moves available
        """
        pass


class StudentAgent(BaseAgent):
    """
    Student Agent Implementation using C++ backend
    
    This agent uses the complete C++ implementation with all game utilities,
    move generation, board evaluation, and alpha-beta pruning algorithm.
    """
    
    def __init__(self, player: str):
        super().__init__(player)
        
        # Initialize the C++ agent
        self.agent = s1.StudentAgent(player)
        
        # Add predefined move list to match Python version exactly
        if player == "circle":
            # if in_bounds(4, 9, )
            self._mv_list = [
                {'action': 'flip', 'from': [4, 9], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [7, 9], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [8, 9], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [3, 9], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [3, 8], 'orientation': 'vertical'},
                {'action': 'flip', 'from': [8, 8], 'orientation': 'vertical'},
                {'action': 'move', 'from': [8, 8], 'to': [11, 9]},
                {'action': 'move', 'from': [3, 8], 'to': [0, 9]},
                {'action': 'move', 'from': [8, 9], 'to': [11, 0]},
                {'action': 'move', 'from': [3, 9], 'to': [0, 0]},
                {'action': 'move', 'from': [7, 8], 'to': [6, 0]},
                {'action': 'move', 'from': [4, 8], 'to': [5, 0]},
                {'action': 'move', 'from': [6, 9], 'to': [7, 0]},
                {'action': 'move', 'from': [5, 9], 'to': [4, 0]}
            ]
        else:
            self._mv_list = [
                {'action': 'flip', 'from': [8, 3], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [3, 3], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [4, 3], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [7, 3], 'orientation': 'horizontal'},
                {'action': 'flip', 'from': [8, 4], 'orientation': 'vertical'},
                {'action': 'flip', 'from': [3, 4], 'orientation': 'vertical'},
                {'action': 'move', 'from': [8, 4], 'to': [11, 3]},
                {'action': 'move', 'from': [3, 4], 'to': [0, 3]},
                {'action': 'move', 'from': [8, 3], 'to': [11, 12]},
                {'action': 'move', 'from': [3, 3], 'to': [0, 12]},
                {'action': 'move', 'from': [7, 4], 'to': [6, 12]},
                {'action': 'move', 'from': [4, 4], 'to': [5, 12]},
                {'action': 'move', 'from': [6, 3], 'to': [7, 12]},
                {'action': 'move', 'from': [5, 3], 'to': [4, 12]}
            ]
        
        print(f"Initialized StudentAgent for {player} using C++ backend")

    def _convert_board_to_dict(self, board: List[List[Any]]) -> List[List[Dict[str, str]]]:
        """
        Convert board with Piece objects to dictionary format for C++ module.
        
        Args:
            board: 2D list with Piece objects or None
            
        Returns:
            2D list with dictionaries or empty dictionaries
        """
        converted = []
        for row in board:
            converted_row = []
            for cell in row:
                if cell is None:
                    converted_row.append({})
                else:
                    # Convert Piece object to dictionary
                    piece_dict = {
                        "owner": cell.owner,
                        "side": cell.side
                    }
                    if hasattr(cell, 'orientation') and cell.orientation is not None and cell.orientation != 'None':
                        piece_dict["orientation"] = cell.orientation
                    converted_row.append(piece_dict)
            converted.append(converted_row)
        return converted

    def choose(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int], current_player_time: float, opponent_time: float) -> Optional[Dict[str, Any]]:
        """
        Choose the best move using the C++ implementation with Python simulation.
        
        Args:
            board: 2D list representing the game board
            rows, cols: Board dimensions
            score_cols: List of column indices for scoring areas
            current_player_time: Time remaining for current player
            opponent_time: Time remaining for opponent
        
        Returns:
            Dictionary representing the chosen move, or None if no moves available
        """
        def convert_board(board):
            converted = []
            for row in board:
                new_row = []
                for cell in row:
                    if cell is None:
                        new_row.append({})
                    else:
                        new_row.append({
                            "owner": str(cell.owner),
                            "side": str(cell.side),
                            "orientation": str(cell.orientation) if hasattr(cell, 'orientation') else None
                        })
                converted.append(new_row)
            return converted

        def convert_move(move):
            # {'action': 'flip', 'from': [3, 4], 'orientation': 'vertical'},
            if move is None:
                return None
            if move.action == "flip":
                return {
                    "action": "flip",
                    "from": move.from_pos,
                    "orientation": move.orientation
                }
            if move.action == "move":
                return {
                    "action": "move",
                    "from": move.from_pos,
                    "to": move.to_pos
                }
            if move.action == "push":
                return {
                    "action": "push",
                    "from": move.from_pos,
                    "to": move.to_pos,
                    "pushed_to": move.pushed_to
                }
            if move.action == "rotate": 
                return {
                    "action": "rotate",
                    "from": move.from_pos
                }
            return None



        converted_board = convert_board(board)
        mvcpp = self.agent.choose(converted_board, rows, cols, score_cols, current_player_time, opponent_time)
        print(mvcpp)
        print(convert_move(mvcpp))
        return convert_move(mvcpp)
        if mvcpp is None:
            return None
        # Use Python's move generation and selection logic to match exactly
        from student_agent import generate_all_moves, simulate_move
        import random
        import math
        
        moves = generate_all_moves(board, self.player, rows, cols, score_cols)
        if not moves:
            return None
            
        # Check if we have predefined moves (matching Python logic exactly)
        if hasattr(self, '_mv_list') and self._mv_list:
            mv = self._mv_list.pop(0)
            if mv['action'] == "move":
                # Use Python's check_move logic
                from student_agent import agent_compute_valid_moves
                possible_moves = agent_compute_valid_moves(board, mv['from'][0], mv['from'][1], self.player, rows, cols, score_cols)
                success = tuple(mv['to']) in possible_moves['moves']
                if not success:
                    while self._mv_list:
                        mv = self._mv_list.pop(0)
                        possible_moves = agent_compute_valid_moves(board, mv['from'][0], mv['from'][1], self.player, rows, cols, score_cols)
                        success = tuple(mv['to']) in possible_moves['moves']
                        if success:
                            return mv
                if not success:
                    return random.choice(moves)
            return mv
        
        # Use C++ for alpha-beta search with Python simulation
        best_move = moves[0]
        best_value = -math.inf
        alpha = -math.inf
        beta = math.inf

        for move in moves:
            success, new_board = simulate_move(board, move, self.player, rows, cols, score_cols)
            if not success:
                continue

            # Use C++ for board evaluation
            converted_board = self._convert_board_to_dict(new_board)
            board_value = s1.basic_evaluate_board(converted_board, self.player, rows, cols, score_cols)

            if board_value > best_value:
                best_value = board_value
                best_move = move
            
            alpha = max(alpha, best_value)

        return best_move
    
    def check_move(self, board: List[List[Any]], move: Dict[str, Any], rows: int, cols: int, score_cols: List[int]) -> bool:
        """
        Check if a move is valid using the C++ implementation.
        
        Args:
            board: Current board state
            move: Move to check
            rows, cols: Board dimensions
            score_cols: List of column indices for scoring areas
        
        Returns:
            True if move is valid, False otherwise
        """
        # Convert board to dictionary format
        converted_board = self._convert_board_to_dict(board)
        
        # Convert Python move dict to C++ Move object
        cpp_move = s1.Move(
            move["action"],
            move["from"],
            move["to"],
            move.get("pushed_to", []),
            move.get("orientation", "")
        )
        
        return self.agent.check_move(converted_board, cpp_move, self.player, rows, cols, score_cols)
    
    def evaluate_board(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int]) -> float:
        """
        Evaluate the board position using the C++ implementation.
        
        Args:
            board: Current board state
            rows, cols: Board dimensions
            score_cols: List of column indices for scoring areas
        
        Returns:
            Board evaluation score
        """
        converted_board = self._convert_board_to_dict(board)
        return s1.basic_evaluate_board(converted_board, self.player, rows, cols, score_cols)
    
    def generate_moves(self, board: List[List[Any]], rows: int, cols: int, score_cols: List[int]) -> List[Dict[str, Any]]:
        """
        Generate all valid moves using the C++ implementation.
        
        Args:
            board: Current board state
            rows, cols: Board dimensions
            score_cols: List of column indices for scoring areas
        
        Returns:
            List of all valid moves
        """
        converted_board = self._convert_board_to_dict(board)
        cpp_moves = s1.generate_all_moves(converted_board, self.player, rows, cols, score_cols)
        
        # Convert C++ moves to Python dictionaries
        moves = []
        for cpp_move in cpp_moves:
            move_dict = {
                "action": cpp_move.action,
                "from": cpp_move.from_pos,
                "to": cpp_move.to_pos,
            }
            
            if cpp_move.action == "push":
                move_dict["pushed_to"] = cpp_move.pushed_to
            if cpp_move.action == "flip":
                move_dict["orientation"] = cpp_move.orientation
                
            moves.append(move_dict)
        
        return moves


def test_student_agent():
    """
    Basic test to verify the student agent can be created and make moves.
    """
    print("Testing StudentAgent with C++ backend...")
    
    try:
        from gameEngine import default_start_board, DEFAULT_ROWS, DEFAULT_COLS
        
        rows, cols = DEFAULT_ROWS, DEFAULT_COLS
        score_cols = score_cols_for(cols)
        board = default_start_board(rows, cols)
        
        # Test both players
        for player in ["circle", "square"]:
            print(f"\nTesting {player} player...")
            agent = StudentAgent(player)
            
            # Test move generation
            moves = agent.generate_moves(board, rows, cols, score_cols)
            print(f"Generated {len(moves)} moves for {player}")
            
            # Test board evaluation
            score = agent.evaluate_board(board, rows, cols, score_cols)
            print(f"Board evaluation for {player}: {score}")
            
            # Test move selection
            move = agent.choose(board, rows, cols, score_cols, 1.0, 1.0)
            if move:
                print(f"✓ {player} agent successfully generated a move: {move['action']}")
            else:
                print(f"✗ {player} agent returned no move")
        
        print("\n✓ All tests passed!")
        
    except ImportError as e:
        print(f"Could not import gameEngine: {e}")
        print("Creating agent without game engine test...")
        
        agent = StudentAgent("circle")
        print("✓ StudentAgent created successfully with C++ backend")
        
    except Exception as e:
        print(f"Error during testing: {e}")
        print("This might be due to the C++ module not being built yet.")
        print("Please run the build script first.")


if __name__ == "__main__":
    # Run basic test when file is executed directly
    test_student_agent()
