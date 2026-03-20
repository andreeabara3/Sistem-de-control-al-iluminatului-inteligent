library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity pwm is
    generic ( 
        G_WIDTH: integer := 12
    );
    Port (
    clk: in std_logic; 
    duty: in unsigned (G_WIDTH - 1 downto 0);  --factorul de umplere (0-4095)
    pwm_o: out std_logic  -- semnalul pwm pentru led
    ); 
end pwm;

architecture Behavioral of pwm is

signal cnt: unsigned (G_WIDTH - 1 downto 0) := (others => '0');
begin
    process(clk)
    begin
        if rising_edge(clk) then
                cnt <= cnt + 1;
        end if;
    end process;
    
    pwm_o <= '1' when cnt < duty else '0';

end Behavioral;
